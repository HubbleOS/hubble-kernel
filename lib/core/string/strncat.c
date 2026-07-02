/**
 * @file strncat.c
 * @brief Bounded string concatenation
 */

#include <hubble/string.h>

/**
 * @brief Append at most n characters from a string
 * @param dest Destination string
 * @param src Source string to append
 * @param n Maximum number of characters to append
 * @return Pointer to dest
 */
char *strncat(char *dest, const char *src, size_t n) {
  char *a = dest;

  dest += strlen(dest);
  while (n-- && *src)
    *dest++ = *src++;

  *dest = '\0';

  return a;
}
