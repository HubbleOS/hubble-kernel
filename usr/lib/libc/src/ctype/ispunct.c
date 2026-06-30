/**
 * @file ispunct.c
 * @brief Character classification: ispunct
 */

#include <ctype.h>

int ispunct(int c) { return isgraph(c) && !isalnum(c); }
