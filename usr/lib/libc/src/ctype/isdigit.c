/**
 * @file isdigit.c
 * @brief Character classification: isdigit
 */

#include <ctype.h>

int isdigit(int c) { return (unsigned)c - '0' < 10; }
