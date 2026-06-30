/**
 * @file isalnum.c
 * @brief Character classification: isalnum
 */

#include <ctype.h>

int isalnum(int c) { return isalpha(c) || isdigit(c); }
