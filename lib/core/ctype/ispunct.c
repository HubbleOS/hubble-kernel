/**
 * @file ispunct.c
 * @brief Character classification: punctuation test
 */

#include <hubble/ctype.h>

/**
 * @brief Check if character is punctuation
 * @param c Character to test
 * @return Non-zero if punctuation, 0 otherwise
 */
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
