/**
 * @file isspace.c
 * @brief Character classification: whitespace test
 */

#include <hubble/ctype.h>

/**
 * @brief Check if character is whitespace
 * @param c Character to test
 * @return Non-zero if whitespace, 0 otherwise
 */
int isspace(int c) { return c == ' ' || (unsigned)c - '\t' < 5; }
