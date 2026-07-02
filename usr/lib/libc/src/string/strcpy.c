/**
 * @file strcpy.c
 * @brief Copy a string
 */

#include <string.h>

char *strcpy(char *dest, const char *src) {
  char *orig = dest;
  while ((*dest++ = *src++))
    ;
  return orig;
}
