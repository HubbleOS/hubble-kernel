/**
 * @file stream.h
 * @brief A one-way byte stream: the core of pipes and stream sockets
 *
 * A bounded byte queue between a writing side and a reading side. Reads
 * block until data arrives and return what is there (0 = end of file
 * once the writing side is closed); writes block until everything is
 * queued (-EPIPE once the reading side is closed). Non-blocking callers
 * get -EAGAIN instead of a wait. User buffers are only touched with the
 * lock dropped, since they may fault.
 *
 * The owner supplies the wait queue, so one queue can serve several
 * streams (a socket's two directions), and frees the stream.
 */
#pragma once

#include <smp/spinlock.h>
#include <smp/waitqueue.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  spinlock_t lock;
  wait_queue_t *wq; /* the owner's: woken on every change */
  uint8_t *buf;
  size_t size;
  size_t head, count; /* next byte to read, bytes queued */
  uint64_t written;   /* bytes ever queued: positions for ancillary data */
  bool reader_open, writer_open;
} stream_t;

/** Set up an empty stream of @p size bytes. Returns 0 or -ENOMEM. */
int stream_init(stream_t *s, size_t size, wait_queue_t *wq);

/** Free the buffer (both sides must be done with the stream). */
void stream_destroy(stream_t *s);

/** Wait until a read would not block (data queued, or EOF). Returns 0,
 * or -EAGAIN if it would have to wait and @p nonblock is set. */
long stream_wait_readable(stream_t *s, bool nonblock);

/** Read up to @p len bytes. Returns bytes read, 0 at EOF, or -errno. */
long stream_read(stream_t *s, void *buf, size_t len, bool nonblock);

/** Write all of @p buf. Returns bytes written (short only if
 * non-blocking or the reader went away midway) or -errno. */
long stream_write(stream_t *s, const void *buf, size_t len, bool nonblock);

/** FILE_POLLIN / FILE_POLLHUP for the reading side. */
short stream_read_poll(stream_t *s);

/** FILE_POLLOUT / FILE_POLLERR for the writing side. */
short stream_write_poll(stream_t *s);

/**
 * Close one side (idempotent) and wake the other: it sees EOF or EPIPE.
 * Returns true if this closed the last open side, so the caller can free
 * the stream. While the other side is open the wake happens under the
 * lock, as the other side's close may free the stream once it drops.
 */
bool stream_close(stream_t *s, bool reader);
