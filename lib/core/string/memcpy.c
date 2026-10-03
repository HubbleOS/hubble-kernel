/**
 * @file memcpy.c
 * @brief Memory copy (non-overlapping)
 */

#include <hubble/string.h>
#include <stdint.h>

/**
 * @brief Copy memory region (non-overlapping)
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Number of bytes to copy
 * @return Pointer to dest
 */
void *memcpy(void *dest, const void *src, size_t n) {
  unsigned char *d = dest;
  const unsigned char *s = src;

  if ((((uintptr_t)d ^ (uintptr_t)s) & (sizeof(uint64_t) - 1)) == 0) {
    while (((uintptr_t)d & (sizeof(uint64_t) - 1)) && n) {
      *d++ = *s++;
      n--;
    }

    uint64_t *dw = (uint64_t *)d;
    const uint64_t *sw = (const uint64_t *)s;
    while (n >= sizeof(uint64_t)) {
      *dw++ = *sw++;
      n -= sizeof(uint64_t);
    }
    d = (unsigned char *)dw;
    s = (const unsigned char *)sw;
  }

  while (n--)
    *d++ = *s++;
  return dest;
}
