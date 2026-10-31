/*
 * Syscall: futex, for processes with a single thread.
 *
 * A futex is a wait queue keyed by a user address. With no threads (clone
 * shares no address space yet) nothing in the process can wake a waiter,
 * so there is no queue: FUTEX_WAKE finds nobody, and a FUTEX_WAIT whose
 * value still matches sleeps until its timeout or a signal - a lock the
 * only thread already holds, which would hang on Linux too. glibc uses
 * futexes for call_once and locks even in single-threaded programs, and
 * aborts when the syscall is missing.
 */

#include <hpet/hpet.h>
#include <hubble/errno.h>
#include <hubble/syscalls.h>

#include "syscall_entry.h"

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_REQUEUE 3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAKE_OP 5
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAKE_BITSET 10
#define FUTEX_PRIVATE_FLAG 128
#define FUTEX_CLOCK_REALTIME 256

#define USER_END 0x0000800000000000ULL

/* Sleep for the rest of @p timeout (relative, or absolute for
 * FUTEX_WAIT_BITSET), or until a signal when there is none. */
static long futex_sleep(int cmd, const struct kernel_timespec *timeout) {
  if (!timeout)
    return sys_pause(); /* -EINTR once a signal arrives */

  struct kernel_timespec rel = *timeout;
  if (cmd == FUTEX_WAIT_BITSET) { /* absolute: clocks count from boot */
    int64_t now = (int64_t)hpet_get_time_ns();
    int64_t end = rel.tv_sec * (int64_t)NS_PER_SEC + rel.tv_nsec;
    if (end <= now)
      return -ETIMEDOUT;
    rel.tv_sec = (end - now) / NS_PER_SEC;
    rel.tv_nsec = (end - now) % NS_PER_SEC;
  }
  long r = sys_nanosleep(&rel, NULL);
  return r == 0 ? -ETIMEDOUT : -EINTR;
}

long sys_futex(uint32_t *uaddr, int op, uint32_t val,
               const struct kernel_timespec *timeout, uint32_t *uaddr2,
               uint32_t val3) {
  (void)uaddr2, (void)val3;
  int cmd = op & ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME);
  if (!uaddr || (uint64_t)uaddr % 4 || (uint64_t)uaddr >= USER_END)
    return -EINVAL;

  switch (cmd) {
  case FUTEX_WAIT:
  case FUTEX_WAIT_BITSET:
    if (*uaddr != val)
      return -EAGAIN;
    return futex_sleep(cmd, timeout);
  case FUTEX_WAKE:
  case FUTEX_WAKE_BITSET:
  case FUTEX_WAKE_OP:
  case FUTEX_REQUEUE:
  case FUTEX_CMP_REQUEUE:
    return 0; /* no waiters */
  default:
    return -ENOSYS;
  }
}
