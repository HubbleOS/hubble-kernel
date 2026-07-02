/**
 * @file isblank.c
 * @brief Character classification: isblank
 */

#include <ctype.h>

int isblank(int c) { return c == ' ' || c == '\t'; }
