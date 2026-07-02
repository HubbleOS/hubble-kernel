/**
 * @file isalpha.c
 * @brief Character classification: alphabetic test
 */

#include <hubble/ctype.h>

/**
 * @brief Check if character is alphabetic
 * @param c Character to test
 * @return Non-zero if alphabetic, 0 otherwise
 */
int isalpha(int c) { return ((unsigned)c | 32) - 'a' < 26; }
