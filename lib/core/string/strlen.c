/**
 * @file strlen.c
 * @brief String length calculation
 */

#include <hubble/string.h>

/**
 * @brief Calculate the length of a string
 * @param s String to measure
 * @return Number of characters before the null terminator
 */
size_t strlen(const char *s) {
  const char *p = s;
  while (*p)
    ++p;
  return p - s;
}
