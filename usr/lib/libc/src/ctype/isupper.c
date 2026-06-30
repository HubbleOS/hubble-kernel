/**
 * @file isupper.c
 * @brief Character classification: isupper
 */

#include <ctype.h>

int isupper(int c) { return (unsigned)c - 'A' < 26; }
