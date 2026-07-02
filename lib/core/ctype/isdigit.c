/**
 * @file isdigit.c
 * @brief Character classification: decimal digit test
 */

#include <hubble/ctype.h>

/**
 * @brief Check if character is a decimal digit
 * @param c Character to test
 * @return Non-zero if digit, 0 otherwise
 */
int isdigit(int c) { return (unsigned)c - '0' < 10; }
