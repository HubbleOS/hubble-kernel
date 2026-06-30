/**
 * @file isgraph.c
 * @brief Character classification: isgraph
 */

#include <ctype.h>

int isgraph(int c) { return (unsigned)c - 0x21 < 0x5e; }
