/**
 * @file fprintf.c
 * @brief Formatted print to a stream
 */

#include <stdio.h>

int fprintf(FILE *stream, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int ret = vfprintf(stream, format, args);
  va_end(args);
  return ret;
}
