/*
 * Syscalls: clock_gettime and gettimeofday.
 *
 * Time comes from the HPET counter, so it is time since boot: there is no
 * RTC read yet, and CLOCK_REALTIME therefore starts at the 1970 epoch.
 */

#include <hpet/hpet.h>
#include <hubble/errno.h>
#include <hubble/syscalls.h>

struct kernel_timespec {
  int64_t tv_sec;
  int64_t tv_nsec;
};

struct kernel_timeval {
  int64_t tv_sec;
  int64_t tv_usec;
};

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_MONOTONIC_RAW 4
#define CLOCK_REALTIME_COARSE 5
#define CLOCK_MONOTONIC_COARSE 6
#define CLOCK_BOOTTIME 7

long sys_clock_gettime(int clock_id, void *ts) {
  switch (clock_id) {
  case CLOCK_REALTIME:
  case CLOCK_MONOTONIC:
  case CLOCK_MONOTONIC_RAW:
  case CLOCK_REALTIME_COARSE:
  case CLOCK_MONOTONIC_COARSE:
  case CLOCK_BOOTTIME:
    break;
  default:
    return -EINVAL;
  }
  if (!ts)
    return -EFAULT;
  if (!hpet_is_initialized())
    return -ENOSYS;

  uint64_t ns = hpet_get_time_ns();
  struct kernel_timespec *out = ts;
  out->tv_sec = ns / 1000000000ULL;
  out->tv_nsec = ns % 1000000000ULL;
  return 0;
}

long sys_gettimeofday(void *tv, void *tz) {
  (void)tz; /* obsolete; Linux ignores it too unless asked for zone data */
  if (!tv)
    return 0;
  if (!hpet_is_initialized())
    return -ENOSYS;

  uint64_t ns = hpet_get_time_ns();
  struct kernel_timeval *out = tv;
  out->tv_sec = ns / 1000000000ULL;
  out->tv_usec = (ns % 1000000000ULL) / 1000;
  return 0;
}
