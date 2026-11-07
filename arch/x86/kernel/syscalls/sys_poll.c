/*
 * Syscalls that wait with a timeout: poll, ppoll, select, pselect6 and
 * nanosleep. epoll (sys_epoll.c) waits through poll_fds() too.
 */

#include <hpet/hpet.h>
#include <hubble/errno.h>
#include <hubble/signal.h>
#include <hubble/string.h>
#include <hubble/syscalls.h>

#include <fs/vfs/dev.h>
#include <fs/vfs/file.h>
#include <smp/fdtable.h>
#include <smp/scheduler.h>
#include <smp/task.h>
#include <smp/waitqueue.h>

#include "poll.h"
#include "syscall_entry.h"

#define NS_PER_MS 1000000ULL
#define NS_PER_US 1000ULL
/* How often a wait looks again at files that cannot wake it. */
#define POLL_BLIND_NS (10 * NS_PER_MS)

typedef struct {
  struct pollfd *fds;
  unsigned long nfds;
  long ready;
} poll_ctx_t;

/* What an fd can do right now (the open file decides; files without a
 * poll hook are always ready, as on Linux). */
static short fd_ready_events(int fd) {
  file_t *file = fd_file(get_current_task(), fd);
  if (!file)
    return POLLNVAL;
  return file->ops->poll ? file->ops->poll(file) : POLLIN | POLLOUT;
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

/* The wait queues of every watched file (a tty's input queue, a pipe,
 * a socket), each once. Returns how many; *blind is set if a file can
 * change its answer without waking anyone (a poll hook, no queue). */
static size_t poll_wait_queues(poll_ctx_t *ctx, wait_queue_t **wqs,
                               bool *blind) {
  size_t n = 0;
  *blind = false;
  for (unsigned long i = 0; i < ctx->nfds; i++) {
    if (ctx->fds[i].fd < 0)
      continue;
    file_t *file = fd_file(get_current_task(), ctx->fds[i].fd);
    if (!file || !file->ops->poll)
      continue;
    wait_queue_t *wq = file->ops->wait_queue
                           ? (wait_queue_t *)file->ops->wait_queue(file)
                           : NULL;
    if (!wq) {
      *blind = true;
      continue;
    }
    size_t j = 0;
    while (j < n && wqs[j] != wq)
      j++;
    if (j == n)
      wqs[n++] = wq;
  }
  return n;
}

/* Sleep until @p deadline (HPET ns); true if a signal cut it short. */
static bool sleep_until(uint64_t deadline) {
  while (hpet_get_time_ns() < deadline) {
    task_prepare_wait();
    if (signal_pending()) {
      task_cancel_wait();
      return true;
    }
    task_wait(deadline);
  }
  return false;
}

long poll_fds(struct pollfd *fds, unsigned long nfds, uint64_t deadline,
              bool nowait) {
  poll_ctx_t ctx = {.fds = fds, .nfds = nfds, .ready = 0};
  if (poll_scan(&ctx) || nowait)
    return ctx.ready;

  wait_queue_t *wqs[MAX_FDS];
  bool blind;
  size_t nwqs = poll_wait_queues(&ctx, wqs, &blind);

  for (;;) {
    /* With a blind file in the set, wake now and then to look again. */
    uint64_t until = deadline;
    if (blind) {
      uint64_t tick = hpet_get_time_ns() + POLL_BLIND_NS;
      if (!until || tick < until)
        until = tick;
    }

    long err;
    if (nwqs) {
      err = waitqueue_wait_any(wqs, nwqs, poll_scan, &ctx, until);
    } else {
      /* Nothing can wake us early: a plain sleep. */
      err = sleep_until(until ? until : UINT64_MAX) ? -ERESTARTSYS : 0;
      poll_scan(&ctx);
    }
    /* Like Linux: these calls are not restarted, a signal ends them. */
    if (err == -ERESTARTSYS)
      return -EINTR;
    if (ctx.ready || (deadline && hpet_get_time_ns() >= deadline))
      return ctx.ready;
  }
}

uint64_t poll_deadline(int64_t sec, int64_t nsec) {
  return hpet_get_time_ns() + (uint64_t)sec * NS_PER_SEC + (uint64_t)nsec;
}

void poll_time_left(uint64_t deadline, int64_t *sec, int64_t *frac,
                    bool usec) {
  uint64_t now = hpet_get_time_ns();
  uint64_t left = now < deadline ? deadline - now : 0;
  *sec = (int64_t)(left / NS_PER_SEC);
  *frac = (int64_t)(left % NS_PER_SEC / (usec ? NS_PER_US : 1));
}

/* A blocked-signals mask for the call: ppoll's, pselect6's. */
static long temp_sigmask(const uint64_t *mask, size_t size) {
  if (!mask)
    return 0;
  if (size != sizeof(uint64_t))
    return -EINVAL;
  sig_set_temp_mask(&get_current_task()->signals, *mask);
  return 0;
}

static bool timespec_valid(const struct kernel_timespec *ts) {
  return ts->tv_sec >= 0 && ts->tv_nsec >= 0 &&
         ts->tv_nsec < (int64_t)NS_PER_SEC;
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
  uint64_t deadline =
      timeout_ms > 0 ? poll_deadline(0, timeout_ms * (int64_t)NS_PER_MS) : 0;
  return poll_fds(fds, nfds, deadline, timeout_ms == 0);
}

/* ppoll: poll with a timespec (NULL: forever, written back with what is
 * left) and a signal mask for the wait. */
long sys_ppoll(struct pollfd *fds, unsigned long nfds,
               struct kernel_timespec *ts, const uint64_t *sigmask,
               size_t sigsetsize) {
  if (nfds > MAX_FDS)
    return -EINVAL;
  if (nfds && !fds)
    return -EFAULT;
  if (ts && !timespec_valid(ts))
    return -EINVAL;
  long err = temp_sigmask(sigmask, sigsetsize);
  if (err)
    return err;

  bool now = ts && !ts->tv_sec && !ts->tv_nsec;
  uint64_t deadline = ts && !now ? poll_deadline(ts->tv_sec, ts->tv_nsec) : 0;
  long ret = poll_fds(fds, nfds, deadline, now);
  if (ts && deadline)
    poll_time_left(deadline, &ts->tv_sec, &ts->tv_nsec, false);
  return ret;
}

/* -- select ---------------------------------------------------------- */

#define FDSET_BITS 64
typedef struct {
  uint64_t bits[(MAX_FDS + FDSET_BITS - 1) / FDSET_BITS];
} fdset_t;

static bool fdset_has(const uint64_t *set, int fd) {
  return set && (set[fd / FDSET_BITS] >> (fd % FDSET_BITS)) & 1;
}

/* The select sets as a pollfd array: a closed fd in them is EBADF. */
static long select_core(int nfds, uint64_t *r, uint64_t *w, uint64_t *e,
                        uint64_t deadline, bool now) {
  if (nfds < 0)
    return -EINVAL;
  /* As Linux: bits past the fd table are ignored, and the sets are only
   * read and written that far. */
  if (nfds > MAX_FDS)
    nfds = MAX_FDS;

  struct pollfd pfds[MAX_FDS];
  unsigned long n = 0;
  for (int fd = 0; fd < nfds; fd++) {
    short ev = (fdset_has(r, fd) ? POLLIN : 0) |
               (fdset_has(w, fd) ? POLLOUT : 0) |
               (fdset_has(e, fd) ? POLLPRI : 0);
    if (!ev)
      continue;
    if (!fd_file(get_current_task(), fd))
      return -EBADF;
    pfds[n++] = (struct pollfd){.fd = fd, .events = ev};
  }

  long ret = poll_fds(pfds, n, deadline, now);
  if (ret < 0)
    return ret;

  /* Linux's mapping: readable includes hangups and errors, writable
   * includes errors; the result counts set bits. */
  size_t words = ((size_t)nfds + FDSET_BITS - 1) / FDSET_BITS;
  fdset_t ro = {0}, wo = {0}, eo = {0};
  long count = 0;
  for (unsigned long i = 0; i < n; i++) {
    int fd = pfds[i].fd;
    short rev = pfds[i].revents;
    uint64_t bit = 1ULL << (fd % FDSET_BITS);
    if ((pfds[i].events & POLLIN) && (rev & (POLLIN | POLLHUP | POLLERR)))
      ro.bits[fd / FDSET_BITS] |= bit, count++;
    if ((pfds[i].events & POLLOUT) && (rev & (POLLOUT | POLLERR)))
      wo.bits[fd / FDSET_BITS] |= bit, count++;
    if ((pfds[i].events & POLLPRI) && (rev & POLLPRI))
      eo.bits[fd / FDSET_BITS] |= bit, count++;
  }
  if (r)
    memcpy(r, ro.bits, words * sizeof(uint64_t));
  if (w)
    memcpy(w, wo.bits, words * sizeof(uint64_t));
  if (e)
    memcpy(e, eo.bits, words * sizeof(uint64_t));
  return count;
}

struct kernel_timeval {
  int64_t tv_sec;
  int64_t tv_usec;
};

/* select: the timeout is a timeval, written back with what is left. */
long sys_select(int nfds, uint64_t *r, uint64_t *w, uint64_t *e,
                struct kernel_timeval *tv) {
  if (tv && (tv->tv_sec < 0 || tv->tv_usec < 0 || tv->tv_usec >= 1000000))
    return -EINVAL;
  bool now = tv && !tv->tv_sec && !tv->tv_usec;
  uint64_t deadline =
      tv && !now ? poll_deadline(tv->tv_sec, tv->tv_usec * (int64_t)NS_PER_US)
                 : 0;
  long ret = select_core(nfds, r, w, e, deadline, now);
  if (tv && deadline)
    poll_time_left(deadline, &tv->tv_sec, &tv->tv_usec, true);
  return ret;
}

/* pselect6: a timespec, and its sixth argument points at the mask and
 * its size. */
long sys_pselect6(int nfds, uint64_t *r, uint64_t *w, uint64_t *e,
                  struct kernel_timespec *ts, const void *sig) {
  if (ts && !timespec_valid(ts))
    return -EINVAL;
  if (sig) {
    const struct {
      const uint64_t *mask;
      size_t size;
    } *s = sig;
    long err = temp_sigmask(s->mask, s->size);
    if (err)
      return err;
  }
  bool now = ts && !ts->tv_sec && !ts->tv_nsec;
  uint64_t deadline = ts && !now ? poll_deadline(ts->tv_sec, ts->tv_nsec) : 0;
  long ret = select_core(nfds, r, w, e, deadline, now);
  if (ts && deadline)
    poll_time_left(deadline, &ts->tv_sec, &ts->tv_nsec, false);
  return ret;
}

long sys_nanosleep(const struct kernel_timespec *req,
                   struct kernel_timespec *rem) {
  if (!req || req->tv_sec < 0 || req->tv_nsec < 0 ||
      req->tv_nsec >= (int64_t)NS_PER_SEC)
    return -EINVAL;
  if (!hpet_is_initialized())
    return -ENOSYS;

  uint64_t deadline =
      hpet_get_time_ns() + (uint64_t)req->tv_sec * NS_PER_SEC + req->tv_nsec;
  if (!sleep_until(deadline))
    return 0;

  /* Interrupted. A handler that runs sees EINTR and rem; a stop (Ctrl-Z,
   * then fg) continues the sleep through restart_syscall. */
  if (rem) {
    uint64_t now = hpet_get_time_ns();
    uint64_t left = now < deadline ? deadline - now : 0;
    rem->tv_sec = (int64_t)(left / NS_PER_SEC);
    rem->tv_nsec = (int64_t)(left % NS_PER_SEC);
  }
  sig_set_restart(&get_current_task()->signals, deadline);
  return -ERESTART_RESTARTBLOCK;
}

/* restart_syscall: a nanosleep that a signal without a handler cut short
 * goes on to its deadline. */
long sys_restart_syscall(void) {
  uint64_t deadline = sig_restart(&get_current_task()->signals);
  if (!deadline || !sleep_until(deadline))
    return 0;
  return -ERESTART_RESTARTBLOCK;
}
