/**
 * @file pipe.c
 * @brief Anonymous pipes for pipe(2): a bounded byte queue between a read
 *        end and a write end
 *
 * Reads block until data arrives and return what is there (0 = end of
 * file once every write end is closed); writes block until everything is
 * queued (-EPIPE once every read end is closed). O_NONBLOCK turns a would-
 * be wait into -EAGAIN. User buffers are only touched with the pipe lock
 * dropped, since they may fault.
 */
#include "file.h"
#include <hubble/errno.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <smp/spinlock.h>
#include <smp/waitqueue.h>

#define PIPE_SIZE 4096
#define PIPE_CHUNK 256 /* bytes moved per lock hold */

typedef struct {
  spinlock_t lock;
  wait_queue_t wq; /* readers and writers: woken on every change */
  uint8_t buf[PIPE_SIZE];
  size_t head, count; /* next byte to read, bytes queued */
  bool reader_open, writer_open;
} pipe_t;

/* -- Wait conditions -------------------------------------- */

static bool pipe_can_read(void *arg) {
  pipe_t *pipe = arg;
  return pipe->count || !pipe->writer_open;
}

static bool pipe_can_write(void *arg) {
  pipe_t *pipe = arg;
  return pipe->count < PIPE_SIZE || !pipe->reader_open;
}

/* -- Read end --------------------------------------------- */

static long pipe_read(file_t *file, void *buf, size_t len) {
  pipe_t *pipe = file->priv;
  if (!len)
    return 0;

  if (!pipe_can_read(pipe)) {
    if (file->flags & FILE_O_NONBLOCK)
      return -EAGAIN;
    waitqueue_wait_event(&pipe->wq, pipe_can_read, pipe, 0);
  }

  /* Return whatever is queued now; don't wait for more. */
  size_t done = 0;
  while (done < len) {
    uint8_t chunk[PIPE_CHUNK];
    size_t n = 0;

    spinlock_acquire(&pipe->lock);
    while (n < sizeof(chunk) && done + n < len && pipe->count) {
      chunk[n++] = pipe->buf[pipe->head];
      pipe->head = (pipe->head + 1) % PIPE_SIZE;
      pipe->count--;
    }
    spinlock_release(&pipe->lock);

    if (!n)
      break;
    memcpy((uint8_t *)buf + done, chunk, n);
    done += n;
  }

  if (done)
    waitqueue_wake_all(&pipe->wq);
  return (long)done;
}

static short pipe_read_poll(file_t *file) {
  pipe_t *pipe = file->priv;
  return (pipe->count ? FILE_POLLIN : 0) |
         (pipe->writer_open ? 0 : FILE_POLLHUP);
}

/* -- Write end -------------------------------------------- */

static long pipe_write(file_t *file, const void *buf, size_t len) {
  pipe_t *pipe = file->priv;
  size_t done = 0;

  while (done < len) {
    if (!pipe_can_write(pipe)) {
      if (file->flags & FILE_O_NONBLOCK)
        return done ? (long)done : -EAGAIN;
      waitqueue_wait_event(&pipe->wq, pipe_can_write, pipe, 0);
    }
    if (!pipe->reader_open)
      return done ? (long)done : -EPIPE;

    uint8_t chunk[PIPE_CHUNK];
    size_t n = len - done < sizeof(chunk) ? len - done : sizeof(chunk);
    memcpy(chunk, (const uint8_t *)buf + done, n);

    /* Another writer may have used the space meanwhile: queue what
     * fits and go round again for the rest. */
    spinlock_acquire(&pipe->lock);
    size_t space = PIPE_SIZE - pipe->count;
    if (n > space)
      n = space;
    for (size_t i = 0; i < n; i++)
      pipe->buf[(pipe->head + pipe->count + i) % PIPE_SIZE] = chunk[i];
    pipe->count += n;
    spinlock_release(&pipe->lock);

    done += n;
    if (n)
      waitqueue_wake_all(&pipe->wq);
  }
  return (long)done;
}

static short pipe_write_poll(file_t *file) {
  pipe_t *pipe = file->priv;
  return (pipe->count < PIPE_SIZE ? FILE_POLLOUT : 0) |
         (pipe->reader_open ? 0 : FILE_POLLERR);
}

/* -- Lifetime --------------------------------------------- */

static struct wait_queue *pipe_wait_queue(file_t *file) {
  return (struct wait_queue *)&((pipe_t *)file->priv)->wq;
}

/* Closing one end wakes the other side (EOF or EPIPE); closing the
 * second frees the pipe. */
static void pipe_release_end(file_t *file, bool read_end) {
  pipe_t *pipe = file->priv;

  spinlock_acquire(&pipe->lock);
  if (read_end)
    pipe->reader_open = false;
  else
    pipe->writer_open = false;
  bool last = !pipe->reader_open && !pipe->writer_open;
  /* Wake under the lock: once it drops, the other end's release may
   * free the pipe. */
  if (!last)
    waitqueue_wake_all(&pipe->wq);
  spinlock_release(&pipe->lock);

  if (last)
    kfree(pipe);
}

static void pipe_release_read(file_t *file) { pipe_release_end(file, true); }
static void pipe_release_write(file_t *file) { pipe_release_end(file, false); }

static const file_ops_t pipe_read_ops = {
    .read = pipe_read,
    .poll = pipe_read_poll,
    .wait_queue = pipe_wait_queue,
    .release = pipe_release_read,
};

static const file_ops_t pipe_write_ops = {
    .write = pipe_write,
    .poll = pipe_write_poll,
    .wait_queue = pipe_wait_queue,
    .release = pipe_release_write,
};

int pipe_create(file_t **read_end, file_t **write_end) {
  pipe_t *pipe = kzalloc(sizeof(*pipe));
  if (!pipe)
    return -ENOMEM;
  spinlock_init(&pipe->lock, "pipe");
  waitqueue_init(&pipe->wq);
  pipe->reader_open = pipe->writer_open = true;

  *read_end = file_alloc(&pipe_read_ops, FILE_O_RDONLY, pipe);
  *write_end = file_alloc(&pipe_write_ops, FILE_O_WRONLY, pipe);
  if (!*read_end || !*write_end) {
    if (*read_end)
      kfree(*read_end);
    if (*write_end)
      kfree(*write_end);
    kfree(pipe);
    return -ENOMEM;
  }
  return 0;
}
