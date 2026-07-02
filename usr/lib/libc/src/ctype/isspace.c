/**
 * @file isspace.c
 * @brief Character classification: isspace
 */

#include <ctype.h>

int isspace(int c) { return c == ' ' || (unsigned)c - '\t' < 5; }
