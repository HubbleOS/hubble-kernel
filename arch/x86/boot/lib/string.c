/**
 * @file string.c
 * @brief String and memory manipulation implementation for the bootloader
 */

#include "string.h"

/** @brief Compare two memory regions */
int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = s1;
  const unsigned char *p2 = s2;
  while (n--) {
    if (*p1 != *p2)
      return *p1 - *p2;
    p1++;
    p2++;
  }
  return 0;
}

/** @brief Fill memory region with a constant byte */
void *memset(void *dest, int c, size_t n) {
  unsigned char *s = dest;

  if (!n)
    return dest;

  uint8_t byte = (uint8_t)c;

  while (((uintptr_t)s & (sizeof(uint64_t) - 1)) && n) {
    *s++ = byte;
    n--;
  }

  if (n >= sizeof(uint64_t)) {
    uint64_t pattern = byte;
    pattern |= pattern << 8;
    pattern |= pattern << 16;
    pattern |= pattern << 32;

    uint64_t *w = (uint64_t *)s;
    size_t words = n / sizeof(uint64_t);
    for (size_t i = 0; i < words; ++i)
      w[i] = pattern;

    s += words * sizeof(uint64_t);
    n %= sizeof(uint64_t);
  }

  while (n--)
    *s++ = byte;

  return dest;
}

/** @brief Copy memory region */
void *memcpy(void *dest, const void *src, size_t n) {
  unsigned char *d = dest;
  const unsigned char *s = src;
  while (n--)
    *d++ = *s++;
  return dest;
}
