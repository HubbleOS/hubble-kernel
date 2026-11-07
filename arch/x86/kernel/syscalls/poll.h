/*
 * The wait behind poll, ppoll, select, pselect6 and epoll_wait.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

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

/**
 * Fill each revents and, if none is set, wait until one is or @p deadline
 * (HPET ns, 0: none) passes. @p nowait only checks once.
 * @return how many fds have revents, or -EINTR for a signal
 */
long poll_fds(struct pollfd *fds, unsigned long nfds, uint64_t deadline,
              bool nowait);

/** HPET deadline for a relative timeout; 0 if there is none. */
uint64_t poll_deadline(int64_t sec, int64_t nsec);

/** Write what is left until @p deadline to a timespec or timeval. */
void poll_time_left(uint64_t deadline, int64_t *sec, int64_t *frac,
                    bool usec);
