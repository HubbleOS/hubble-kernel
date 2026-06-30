/**
 * @file isxdigit.c
 * @brief Character classification: hexadecimal digit test
 */

#include <hubble/ctype.h>

/**
 * @brief Check if character is a hexadecimal digit
 * @param c Character to test
 * @return Non-zero if hex digit, 0 otherwise
 */
int isxdigit(int c) { return isdigit(c) || ((unsigned)c | 32) - 'a' < 6; }
