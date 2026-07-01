/**
 * @file strnlen.c
 * @brief Get bounded string length
 */

#include <string.h>

size_t strnlen(const char *s, size_t n) {
  const char *p = memchr(s, 0, n);
  return p ? p - s : n;
}
