/**
 * @file memmove.c
 * @brief Memory copy (supports overlapping regions)
 */

#include <hubble/string.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Copy memory region (safe for overlapping regions)
 * @param dest Destination buffer
 * @param src Source buffer
 * @param n Number of bytes to copy
 * @return Pointer to dest
 */
void *memmove(void *dest, const void *src, size_t n) {
  unsigned char *d = dest;
  const unsigned char *s = src;

  if (d == s || n == 0)
    return dest;

  /* Copy in the direction that never overwrites source bytes before
   * they are read: forwards when dest is below src, backwards when it is
   * above. Word-sized while both pointers share alignment (the console
   * scrolls whole framebuffers through here). */
  bool words = (((uintptr_t)d ^ (uintptr_t)s) & (sizeof(uint64_t) - 1)) == 0;

  if (d < s) {
    if (words) {
      while (((uintptr_t)d & (sizeof(uint64_t) - 1)) && n) {
        *d++ = *s++;
        n--;
      }
      for (; n >= sizeof(uint64_t); n -= sizeof(uint64_t)) {
        *(uint64_t *)d = *(const uint64_t *)s;
        d += sizeof(uint64_t);
        s += sizeof(uint64_t);
      }
    }
    while (n--)
      *d++ = *s++;
  } else {
    d += n;
    s += n;
    if (words) {
      while (((uintptr_t)d & (sizeof(uint64_t) - 1)) && n) {
        *--d = *--s;
        n--;
      }
      for (; n >= sizeof(uint64_t); n -= sizeof(uint64_t)) {
        d -= sizeof(uint64_t);
        s -= sizeof(uint64_t);
        *(uint64_t *)d = *(const uint64_t *)s;
      }
    }
    while (n--)
      *--d = *--s;
  }

  return dest;
}
