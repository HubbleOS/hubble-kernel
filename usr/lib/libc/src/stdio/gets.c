/**
 * @file gets.c
 * @brief Read a line from stdin
 */

#include <stdio.h>

char *gets(char *s) { return fgets(s, 256, stdin); }
