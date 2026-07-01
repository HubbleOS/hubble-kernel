/**
 * @file strchr.c
 * @brief Find a character in a string
 */

#include <string.h>

char *strchr(const char *s, int c) {
  unsigned char uc = (unsigned char)c;

  while (*s) {
    if (*s == uc)
      return (char *)s;
    s++;
  }

  return uc == '\0' ? (char *)s : NULL;
}
