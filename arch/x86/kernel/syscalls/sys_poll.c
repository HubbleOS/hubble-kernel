/*
 * Syscalls: poll and nanosleep - the two that wait with a timeout.
 */

#include <hpet/hpet.h>
#include <hubble/errno.h>
#include <hubble/syscalls.h>

#include <fs/vfs/dev.h>
#include <smp/scheduler.h>
#include <smp/task.h>
#include <smp/waitqueue.h>

#include "syscall_entry.h"

struct pollfd {
  int fd;
  short events;
  short revents;
};

#define POLLIN 0x001
#define POLLPRI 0x002
#define POLLOUT 0x004
#define POLLERR 0x008
#define POLLHUP 0x010
#define POLLNVAL 0x020

#define NS_PER_MS 1000000ULL
#define NS_PER_SEC 1000000000ULL

typedef struct {
  struct pollfd *fds;
  unsigned long nfds;
  long ready;
} poll_ctx_t;

/* What an fd can do right now. Character devices answer through their
 * readable() hook; everything else (regular files, pipes - which have
 * no readiness hook yet) is reported ready, as files are on Linux. */
static short fd_ready_events(int fd) {
  if (fd > 2) {
    fd_entry_t *entry = task_get_fd(get_current_task(), fd);
    if (!entry || !entry->data)
      return POLLNVAL;
  }

  VFS_device_reg *dev = fd_get_device(fd);
  if (dev && dev->readable)
    return POLLOUT | (dev->readable() ? POLLIN : 0);
  return POLLIN | POLLOUT;
}

/* Fill revents for every fd; true if any has something to report. */
static bool poll_scan(void *arg) {
  poll_ctx_t *ctx = arg;
  ctx->ready = 0;
  for (unsigned long i = 0; i < ctx->nfds; i++) {
    struct pollfd *p = &ctx->fds[i];
    p->revents = 0;
    if (p->fd < 0)
      continue;
    short mask = fd_ready_events(p->fd);
    p->revents = mask & (p->events | POLLERR | POLLHUP | POLLNVAL);
    if (p->revents)
      ctx->ready++;
  }
  return ctx->ready > 0;
}

/* The one wait queue a poll can sleep on: the first watched device's. */
static wait_queue_t *poll_wait_queue(poll_ctx_t *ctx) {
  for (unsigned long i = 0; i < ctx->nfds; i++) {
    if (ctx->fds[i].fd < 0 || !(ctx->fds[i].events & POLLIN))
      continue;
    VFS_device_reg *dev = fd_get_device(ctx->fds[i].fd);
    if (dev && dev->read_wq)
      return (wait_queue_t *)dev->read_wq;
  }
  return NULL;
}

/**
 * @brief poll: wait until an fd is ready or @p timeout_ms passes.
 * @param timeout_ms <0 waits forever, 0 checks once, >0 is a timeout
 * @return Number of fds with events, 0 on timeout, or negative errno
 */
long sys_poll(struct pollfd *fds, unsigned long nfds, int timeout_ms) {
  if (nfds > MAX_FDS)
    return -EINVAL;
  if (nfds && !fds)
    return -EFAULT;

  poll_ctx_t ctx = {.fds = fds, .nfds = nfds, .ready = 0};
  if (poll_scan(&ctx) || timeout_ms == 0)
    return ctx.ready;

  uint64_t deadline =
      timeout_ms > 0 ? hpet_get_time_ns() + timeout_ms * NS_PER_MS : 0;

  wait_queue_t *wq = poll_wait_queue(&ctx);
  if (wq) {
    waitqueue_wait_event(wq, poll_scan, &ctx, deadline);
    return ctx.ready;
  }

  /* Nothing to wake us early (every watched fd is always-ready or
   * unwatchable): it is a plain sleep, forever only if asked to. */
  if (!deadline)
    return -EINVAL;
  while (hpet_get_time_ns() < deadline) {
    task_prepare_wait();
    task_wait(deadline);
  }
  return 0;
}

struct kernel_timespec {
  int64_t tv_sec;
  int64_t tv_nsec;
};

long sys_nanosleep(const struct kernel_timespec *req,
                   struct kernel_timespec *rem) {
  if (!req || req->tv_sec < 0 || req->tv_nsec < 0 ||
      req->tv_nsec >= (int64_t)NS_PER_SEC)
    return -EINVAL;
  if (!hpet_is_initialized())
    return -ENOSYS;

  uint64_t deadline =
      hpet_get_time_ns() + (uint64_t)req->tv_sec * NS_PER_SEC + req->tv_nsec;
  while (hpet_get_time_ns() < deadline) {
    task_prepare_wait();
    task_wait(deadline);
  }
  if (rem)
    rem->tv_sec = rem->tv_nsec = 0;
  return 0;
}
