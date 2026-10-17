/**
 * @file pipe.c
 * @brief Anonymous pipes for pipe(2): one byte stream (stream.h) between
 *        a read end and a write end
 *
 * O_NONBLOCK on an end turns a would-be wait into -EAGAIN. The pipe is
 * freed when both ends are closed.
 */
#include "file.h"
#include "stream.h"
#include <hubble/errno.h>
#include <mm/kmalloc.h>

#define PIPE_SIZE 4096

typedef struct {
  wait_queue_t wq;
  stream_t stream;
} pipe_t;

static stream_t *pipe_stream(file_t *file) {
  return &((pipe_t *)file->priv)->stream;
}

static bool nonblock(file_t *file) { return file->flags & FILE_O_NONBLOCK; }

static long pipe_read(file_t *file, void *buf, size_t len) {
  return stream_read(pipe_stream(file), buf, len, nonblock(file));
}

static long pipe_write(file_t *file, const void *buf, size_t len) {
  return stream_write(pipe_stream(file), buf, len, nonblock(file));
}

static short pipe_read_poll(file_t *file) {
  return stream_read_poll(pipe_stream(file));
}

static short pipe_write_poll(file_t *file) {
  return stream_write_poll(pipe_stream(file));
}

static struct wait_queue *pipe_wait_queue(file_t *file) {
  return (struct wait_queue *)&((pipe_t *)file->priv)->wq;
}

/* Closing one end wakes the other side (EOF or EPIPE); closing the
 * second frees the pipe. */
static void pipe_release_end(file_t *file, bool read_end) {
  pipe_t *pipe = file->priv;
  if (stream_close(&pipe->stream, read_end)) {
    stream_destroy(&pipe->stream);
    kfree(pipe);
  }
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
  waitqueue_init(&pipe->wq);
  if (stream_init(&pipe->stream, PIPE_SIZE, &pipe->wq) < 0) {
    kfree(pipe);
    return -ENOMEM;
  }

  *read_end = file_alloc(&pipe_read_ops, FILE_O_RDONLY, pipe);
  *write_end = file_alloc(&pipe_write_ops, FILE_O_WRONLY, pipe);
  if (!*read_end || !*write_end) {
    if (*read_end)
      kfree(*read_end);
    if (*write_end)
      kfree(*write_end);
    stream_destroy(&pipe->stream);
    kfree(pipe);
    return -ENOMEM;
  }
  return 0;
}
