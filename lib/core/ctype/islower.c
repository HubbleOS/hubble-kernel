/**
 * @file islower.c
 * @brief Character classification: lowercase letter test
 */

#include <hubble/ctype.h>

/**
 * @brief Check if character is a lowercase letter
 * @param c Character to test
 * @return Non-zero if lowercase, 0 otherwise
 */
int islower(int c) { return (unsigned)c - 'a' < 26; }
