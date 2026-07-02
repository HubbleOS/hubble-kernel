/**
 * @file isupper.c
 * @brief Character classification: uppercase letter test
 */

#include <hubble/ctype.h>

/**
 * @brief Check if character is an uppercase letter
 * @param c Character to test
 * @return Non-zero if uppercase, 0 otherwise
 */
int isupper(int c) { return (unsigned)c - 'A' < 26; }
