/**
 * @file putchar.c
 * @brief Write a character to stdout
 */

#include <stdio.h>

int putchar(int c) { return putc(c, stdout); }
