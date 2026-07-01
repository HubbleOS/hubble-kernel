/**
 * @file printf.c
 * @brief Formatted print to stdout
 */

#include <stdint.h>
#include <stdio.h>

int printf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  int ret = vfprintf(stdout, format, args);
  va_end(args);
  return ret;
}
