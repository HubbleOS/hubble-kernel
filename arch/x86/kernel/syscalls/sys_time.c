/*
 * Syscalls: clock_gettime, gettimeofday and time.
 *
 * Time comes from the HPET counter, time since boot. The wall clock
 * (CLOCK_REALTIME, gettimeofday, time) adds the CMOS RTC's reading,
 * taken once on first use; the RTC is taken to keep UTC, as QEMU's does.
 */

#include <hpet/hpet.h>
#include <io.h>
#include <stdbool.h>
#include <hubble/errno.h>
#include <hubble/syscalls.h>

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

static uint8_t cmos(uint8_t reg) {
  outb(0x70, reg);
  return inb(0x71);
}

/* Days from 1970-01-01 to @p y-@p m-@p d (proleptic Gregorian). */
static int64_t days_from_civil(int64_t y, int m, int d) {
  y -= m <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400;
  int64_t yoe = y - era * 400;
  int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

/* The RTC's time, seconds since the epoch. */
static int64_t rtc_read(void) {
  uint8_t sec, min, hour, day, mon, year, again[6];
  /* Read until two passes agree: an update may land in the middle. */
  do {
    while (cmos(0x0A) & 0x80) /* update in progress */
      ;
    sec = cmos(0x00), min = cmos(0x02), hour = cmos(0x04);
    day = cmos(0x07), mon = cmos(0x08), year = cmos(0x09);
    while (cmos(0x0A) & 0x80)
      ;
    again[0] = cmos(0x00), again[1] = cmos(0x02), again[2] = cmos(0x04);
    again[3] = cmos(0x07), again[4] = cmos(0x08), again[5] = cmos(0x09);
  } while (again[0] != sec || again[1] != min || again[2] != hour ||
           again[3] != day || again[4] != mon || again[5] != year);

  uint8_t b = cmos(0x0B);
  bool pm = hour & 0x80;
  hour &= 0x7F;
  if (!(b & 0x04)) { /* BCD */
#define BCD(v) (((v) & 0x0F) + ((v) >> 4) * 10)
    sec = BCD(sec), min = BCD(min), hour = BCD(hour);
    day = BCD(day), mon = BCD(mon), year = BCD(year);
#undef BCD
  }
  if (!(b & 0x02)) /* 12-hour clock */
    hour = hour % 12 + (pm ? 12 : 0);

  return days_from_civil(2000 + year, mon, day) * 86400 + hour * 3600 +
         min * 60 + sec;
}

/* Wall clock time, ns since the epoch. */
static uint64_t realtime_ns(void) {
  static int64_t boot_sec = -1; /* the epoch time at HPET zero */
  uint64_t ns = hpet_get_time_ns();
  if (boot_sec < 0)
    boot_sec = rtc_read() - (int64_t)(ns / 1000000000ULL);
  return (uint64_t)boot_sec * 1000000000ULL + ns;
}

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

  bool wall = clock_id == CLOCK_REALTIME || clock_id == CLOCK_REALTIME_COARSE;
  uint64_t ns = wall ? realtime_ns() : hpet_get_time_ns();
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

  uint64_t ns = realtime_ns();
  struct kernel_timeval *out = tv;
  out->tv_sec = ns / 1000000000ULL;
  out->tv_usec = (ns % 1000000000ULL) / 1000;
  return 0;
}

/** time(2): whole seconds, also stored in *@p tloc when given. */
long sys_time(int64_t *tloc) {
  if (!hpet_is_initialized())
    return -ENOSYS;
  int64_t sec = realtime_ns() / 1000000000ULL;
  if (tloc)
    *tloc = sec;
  return sec;
}
