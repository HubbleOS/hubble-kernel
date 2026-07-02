/**
 * @file strcmp.c
 * @brief String comparison
 */

#include <hubble/string.h>

/**
 * @brief Compare two strings
 * @param s1 First string
 * @param s2 Second string
 * @return 0 if equal, negative if s1 < s2, positive if s1 > s2
 */
int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2))
    s1++, s2++;

  return *(unsigned char *)s1 - *(unsigned char *)s2;
}
