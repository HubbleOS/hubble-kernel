/**
 * @file strxfrm.c
 * @brief Transform a string for locale-aware comparison (fallback to byte-wise
 * copy)
 */

#include <string.h>

size_t strxfrm(char *dest, const char *src, size_t n) {
  size_t len = strlen(src);

  if (n != 0) {
    size_t copy_len = (len < n - 1) ? len : n - 1;
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
  }

  return len;
}
