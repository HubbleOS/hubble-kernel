/**
 * @file memmove.c
 * @brief Memory copy (supports overlapping regions)
 */

#include <hubble/string.h>

/**
 * @brief Copy memory region (safe for overlapping regions)
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Number of bytes to copy
 * @return Pointer to dest
 */
void *memmove(void *dest, const void *src, size_t n) {
  char *d = dest;
  const char *s = src;

  if (d == s)
    return d;
  if ((uintptr_t)s - (uintptr_t)d - n <= -2 * n)
    return memcpy(d, s, n);

  if (d < s) {
    while (n)
      n--, d[n] = s[n];
  }

  return dest;
}
