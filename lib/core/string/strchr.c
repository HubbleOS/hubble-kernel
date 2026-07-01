/**
 * @file strchr.c
 * @brief String scanning: find first occurrence of a character
 */

#include <hubble/string.h>

/**
 * @brief Find first occurrence of a character in a string
 * @param s String to search
 * @param c Character to find
 * @return Pointer to the first occurrence of c, or NULL if not found
 */
char *strchr(const char *s, int c) {
  unsigned char uc = (unsigned char)c;

  while (*s) {
    if (*s == uc)
      return (char *)s;
    s++;
  }

  return uc == '\0' ? (char *)s : NULL;
}
