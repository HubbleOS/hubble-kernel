/**
 * @file putc.c
 * @brief Write a character to a stream
 */

#include <stdio.h>

int putc(int c, FILE *stream) {
  if (!stream)
    return EOF;

  char ch = (char)c;
  int written = stream->write(stream, &ch, 1);
  if (written != 1)
    return EOF;

  return (unsigned char)c;
}
