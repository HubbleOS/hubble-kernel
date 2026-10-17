/**
 * @file stream.c
 * @brief One-way byte streams for pipes and stream sockets (see stream.h)
 */
#include "stream.h"
#include "file.h"
#include <hubble/errno.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>

#define STREAM_CHUNK 256 /* bytes moved per lock hold */

/* -- Wait conditions -------------------------------------- */

static bool stream_can_read(void *arg) {
  stream_t *s = arg;
  return s->count || !s->writer_open;
}

static bool stream_can_write(void *arg) {
  stream_t *s = arg;
  return s->count < s->size || !s->reader_open;
}

/* -- Lifetime --------------------------------------------- */

int stream_init(stream_t *s, size_t size, wait_queue_t *wq) {
  memset(s, 0, sizeof(*s));
  s->buf = kmalloc(size, GFP_KERNEL);
  if (!s->buf)
    return -ENOMEM;
  spinlock_init(&s->lock, "stream");
  s->wq = wq;
  s->size = size;
  s->reader_open = s->writer_open = true;
  return 0;
}

void stream_destroy(stream_t *s) {
  kfree(s->buf);
  s->buf = NULL;
}

bool stream_close(stream_t *s, bool reader) {
  spinlock_acquire(&s->lock);
  bool *open = reader ? &s->reader_open : &s->writer_open;
  bool was_open = *open;
  *open = false;
  bool last = was_open && !s->reader_open && !s->writer_open;
  if (was_open && !last)
    waitqueue_wake_all(s->wq);
  spinlock_release(&s->lock);
  return last;
}

/* -- Reading ---------------------------------------------- */

long stream_wait_readable(stream_t *s, bool nonblock) {
  if (stream_can_read(s))
    return 0;
  if (nonblock)
    return -EAGAIN;
  waitqueue_wait_event(s->wq, stream_can_read, s, 0);
  return 0;
}

long stream_read(stream_t *s, void *buf, size_t len, bool nonblock) {
  if (!len)
    return 0;

  long err = stream_wait_readable(s, nonblock);
  if (err)
    return err;

  /* Return whatever is queued now; don't wait for more. */
  size_t done = 0;
  while (done < len) {
    uint8_t chunk[STREAM_CHUNK];
    size_t n = 0;

    spinlock_acquire(&s->lock);
    while (n < sizeof(chunk) && done + n < len && s->count) {
      chunk[n++] = s->buf[s->head];
      s->head = (s->head + 1) % s->size;
      s->count--;
    }
    spinlock_release(&s->lock);

    if (!n)
      break;
    memcpy((uint8_t *)buf + done, chunk, n);
    done += n;
  }

  if (done)
    waitqueue_wake_all(s->wq);
  return (long)done;
}

short stream_read_poll(stream_t *s) {
  return (s->count ? FILE_POLLIN : 0) | (s->writer_open ? 0 : FILE_POLLHUP);
}

/* -- Writing ---------------------------------------------- */

long stream_write(stream_t *s, const void *buf, size_t len, bool nonblock) {
  size_t done = 0;

  while (done < len) {
    if (!stream_can_write(s)) {
      if (nonblock)
        return done ? (long)done : -EAGAIN;
      waitqueue_wait_event(s->wq, stream_can_write, s, 0);
    }
    if (!s->reader_open)
      return done ? (long)done : -EPIPE;

    uint8_t chunk[STREAM_CHUNK];
    size_t n = len - done < sizeof(chunk) ? len - done : sizeof(chunk);
    memcpy(chunk, (const uint8_t *)buf + done, n);

    /* Another writer may have used the space meanwhile: queue what
     * fits and go round again for the rest. */
    spinlock_acquire(&s->lock);
    size_t space = s->size - s->count;
    if (n > space)
      n = space;
    for (size_t i = 0; i < n; i++)
      s->buf[(s->head + s->count + i) % s->size] = chunk[i];
    s->count += n;
    s->written += n;
    spinlock_release(&s->lock);

    done += n;
    if (n)
      waitqueue_wake_all(s->wq);
  }
  return (long)done;
}

short stream_write_poll(stream_t *s) {
  return (s->count < s->size ? FILE_POLLOUT : 0) |
         (s->reader_open ? 0 : FILE_POLLERR);
}
