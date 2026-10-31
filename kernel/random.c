/*
 * Random bytes for the kernel and getrandom(): RDRAND when the CPU has it,
 * otherwise xorshift64* stirred with the time stamp counter.
 */

#include <hubble/errno.h>
#include <hubble/random.h>
#include <hubble/string.h>
#include <hubble/syscalls.h>
#include <stdbool.h>
#include <stdint.h>

static uint64_t rdtsc(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return (uint64_t)hi << 32 | lo;
}

static bool has_rdrand(void) {
  static int known = -1;
  if (known < 0) {
    uint32_t a = 1, b, c = 0, d;
    asm volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    known = (c >> 30) & 1;
  }
  return known;
}

static uint64_t random_u64(void) {
  if (has_rdrand()) {
    uint64_t v;
    for (int i = 0; i < 10; i++) { /* RDRAND may fail briefly; retry */
      uint8_t ok;
      asm volatile("rdrand %0; setc %1" : "=r"(v), "=qm"(ok));
      if (ok)
        return v;
    }
  }
  /* Racy across CPUs, which only mixes it further. */
  static uint64_t state = 0x9E3779B97F4A7C15ULL;
  uint64_t x = state ^ rdtsc();
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  state = x;
  return x * 0x2545F4914F6CDD1DULL;
}

void get_random_bytes(void *buf, size_t len) {
  uint8_t *p = buf;
  while (len) {
    uint64_t v = random_u64();
    size_t n = len < sizeof(v) ? len : sizeof(v);
    memcpy(p, &v, n);
    p += n;
    len -= n;
  }
}

#define GRND_NONBLOCK 0x1
#define GRND_RANDOM 0x2
#define GRND_INSECURE 0x4

/** getrandom(2): never blocks, the source is always ready. */
long sys_getrandom(void *buf, size_t len, unsigned int flags) {
  if (flags & ~(GRND_NONBLOCK | GRND_RANDOM | GRND_INSECURE))
    return -EINVAL;
  if (!buf && len)
    return -EFAULT;
  if (len > 0x7FFFFFFF)
    len = 0x7FFFFFFF;
  get_random_bytes(buf, len);
  return (long)len;
}
