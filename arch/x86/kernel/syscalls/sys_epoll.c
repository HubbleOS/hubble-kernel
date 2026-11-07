/*
 * epoll: epoll_create(1), epoll_ctl, epoll_wait and epoll_pwait.
 *
 * An epoll instance is a file holding a list of (fd, open file, events,
 * data). Waiting checks every entry the way poll() does (poll_fds), so it
 * is level triggered; EPOLLET is accepted and reported like level
 * triggering, which edge-triggered programs (they read until EAGAIN)
 * handle fine. EPOLLONESHOT disables an entry once it has reported.
 *
 * Linux drops an entry when its open file is closed. Here an entry
 * remembers the file it was added for (by serial: a closed file's memory
 * may be reused) and is dropped as soon as its fd no longer leads to that
 * file, which is the same for one process.
 */

#include <hubble/errno.h>
#include <hubble/signal.h>
#include <hubble/string.h>
#include <hubble/syscalls.h>

#include <fs/vfs/file.h>
#include <mm/kmalloc.h>
#include <smp/fdtable.h>
#include <smp/scheduler.h>
#include <smp/spinlock.h>
#include <smp/task.h>

#include "poll.h"

#define EPOLL_CLOEXEC 0x80000

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

#define EPOLLIN 0x001
#define EPOLLPRI 0x002
#define EPOLLOUT 0x004
#define EPOLLERR 0x008
#define EPOLLHUP 0x010
#define EPOLLRDNORM 0x040
#define EPOLLRDBAND 0x080
#define EPOLLWRNORM 0x100
#define EPOLLWRBAND 0x200
#define EPOLLMSG 0x400
#define EPOLLRDHUP 0x2000
#define EPOLLEXCLUSIVE (1u << 28)
#define EPOLLWAKEUP (1u << 29)
#define EPOLLONESHOT (1u << 30)
#define EPOLLET (1u << 31)

/* The events poll() can report; the rest are accepted and ignored. */
#define EPOLL_POLL_EVENTS (EPOLLIN | EPOLLPRI | EPOLLOUT | EPOLLERR | EPOLLHUP)

struct epoll_event {
  uint32_t events;
  uint64_t data;
} __attribute__((packed));

typedef struct {
  int fd; /* -1: a free slot */
  uint64_t serial; /* of the open file it was added for */
  uint32_t events;
  uint64_t data;
  bool disabled; /* EPOLLONESHOT, after it reported */
} epoll_item_t;

typedef struct {
  spinlock_t lock;
  epoll_item_t items[MAX_FDS]; /* no more fds than that can be open */
} epoll_t;

static bool item_live(epoll_item_t *it) {
  if (it->fd < 0)
    return false;
  file_t *file = fd_file(get_current_task(), it->fd);
  return file && file->serial == it->serial;
}

/* Copy the entries to wait on into @p pfds (with their slot in @p slot),
 * dropping those whose fd was closed. Under the lock. */
static unsigned long epoll_collect(epoll_t *ep, struct pollfd *pfds,
                                   int *slot) {
  unsigned long n = 0;
  for (int i = 0; i < MAX_FDS; i++) {
    epoll_item_t *it = &ep->items[i];
    if (it->fd < 0)
      continue;
    if (!item_live(it)) {
      it->fd = -1;
      continue;
    }
    if (it->disabled)
      continue;
    pfds[n] = (struct pollfd){
        .fd = it->fd, .events = (short)(it->events & EPOLL_POLL_EVENTS)};
    slot[n++] = i;
  }
  return n;
}

/* An epoll fd is readable when one of its entries is ready (for poll()
 * or another epoll watching it). */
static short epoll_file_poll(file_t *file) {
  epoll_t *ep = file->priv;
  struct pollfd pfds[MAX_FDS];
  int slot[MAX_FDS];
  spinlock_acquire(&ep->lock);
  unsigned long n = epoll_collect(ep, pfds, slot);
  spinlock_release(&ep->lock);
  return poll_fds(pfds, n, 0, true) > 0 ? FILE_POLLIN : 0;
}

static void epoll_release(file_t *file) { kfree(file->priv); }

static const file_ops_t epoll_ops = {
    .poll = epoll_file_poll,
    .release = epoll_release,
};

static epoll_t *epoll_of(int fd, long *err) {
  file_t *file = fd_file(get_current_task(), fd);
  *err = !file ? -EBADF : file->ops != &epoll_ops ? -EINVAL : 0;
  return *err ? NULL : file->priv;
}

long sys_epoll_create1(int flags) {
  if (flags & ~EPOLL_CLOEXEC)
    return -EINVAL;
  epoll_t *ep = kmalloc(sizeof(*ep), GFP_KERNEL);
  if (!ep)
    return -ENOMEM;
  spinlock_init(&ep->lock, "epoll");
  for (int i = 0; i < MAX_FDS; i++)
    ep->items[i].fd = -1;

  file_t *file = file_alloc(&epoll_ops, FILE_O_RDWR, ep);
  if (!file) {
    kfree(ep);
    return -ENOMEM;
  }
  int fd = fd_install(get_current_task(), file, 0,
                      (flags & EPOLL_CLOEXEC) ? FD_CLOEXEC : 0);
  if (fd < 0)
    file_put(file);
  return fd;
}

long sys_epoll_create(int size) {
  return size <= 0 ? -EINVAL : sys_epoll_create1(0);
}

long sys_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
  long err;
  epoll_t *ep = epoll_of(epfd, &err);
  if (!ep)
    return err;
  file_t *file = fd_file(get_current_task(), fd);
  if (!file)
    return -EBADF;
  if (fd == epfd)
    return -EINVAL;
  if (!file->ops->poll || (file->vfs && !file_device(file)))
    return -EPERM; /* regular files: always ready, Linux refuses them */
  if (op != EPOLL_CTL_DEL && !event)
    return -EFAULT;
  struct epoll_event ev = {0};
  if (event)
    ev = *event;

  spinlock_acquire(&ep->lock);
  epoll_item_t *found = NULL, *free_slot = NULL;
  for (int i = 0; i < MAX_FDS; i++) {
    epoll_item_t *it = &ep->items[i];
    if (it->fd >= 0 && !item_live(it))
      it->fd = -1; /* closed since: gone, as on Linux */
    if (it->fd == fd)
      found = it;
    else if (it->fd < 0 && !free_slot)
      free_slot = it;
  }

  err = 0;
  switch (op) {
  case EPOLL_CTL_ADD:
    if (found)
      err = -EEXIST;
    else if (!free_slot)
      err = -ENOSPC;
    else
      *free_slot = (epoll_item_t){.fd = fd,
                                  .serial = file->serial,
                                  .events = ev.events | EPOLLERR | EPOLLHUP,
                                  .data = ev.data};
    break;
  case EPOLL_CTL_MOD:
    if (!found) {
      err = -ENOENT;
    } else {
      found->events = ev.events | EPOLLERR | EPOLLHUP;
      found->data = ev.data;
      found->disabled = false;
    }
    break;
  case EPOLL_CTL_DEL:
    if (!found)
      err = -ENOENT;
    else
      found->fd = -1;
    break;
  default:
    err = -EINVAL;
  }
  spinlock_release(&ep->lock);
  return err;
}

long sys_epoll_pwait(int epfd, struct epoll_event *events, int maxevents,
                     int timeout_ms, const uint64_t *sigmask,
                     size_t sigsetsize) {
  long err;
  epoll_t *ep = epoll_of(epfd, &err);
  if (!ep)
    return err;
  if (maxevents <= 0)
    return -EINVAL;
  if (!events)
    return -EFAULT;
  if (sigmask) {
    if (sigsetsize != sizeof(uint64_t))
      return -EINVAL;
    sig_set_temp_mask(&get_current_task()->signals, *sigmask);
  }

  uint64_t deadline =
      timeout_ms > 0 ? poll_deadline(0, timeout_ms * 1000000LL) : 0;
  struct pollfd pfds[MAX_FDS];
  int slot[MAX_FDS];

  for (;;) {
    spinlock_acquire(&ep->lock);
    unsigned long n = epoll_collect(ep, pfds, slot);
    spinlock_release(&ep->lock);

    /* Entries added or removed meanwhile are seen on the next round. */
    long ready = poll_fds(pfds, n, deadline, timeout_ms == 0);
    if (ready <= 0)
      return ready; /* 0: timed out, or -EINTR */

    int out = 0;
    spinlock_acquire(&ep->lock);
    for (unsigned long i = 0; i < n && out < maxevents; i++) {
      epoll_item_t *it = &ep->items[slot[i]];
      if (!pfds[i].revents || it->fd != pfds[i].fd || it->disabled)
        continue; /* changed by epoll_ctl while we waited */
      events[out].events = (uint32_t)pfds[i].revents & it->events;
      events[out].data = it->data;
      out++;
      if (it->events & EPOLLONESHOT)
        it->disabled = true;
    }
    spinlock_release(&ep->lock);
    if (out)
      return out;
  }
}

long sys_epoll_wait(int epfd, struct epoll_event *events, int maxevents,
                    int timeout_ms) {
  return sys_epoll_pwait(epfd, events, maxevents, timeout_ms, NULL, 0);
}
