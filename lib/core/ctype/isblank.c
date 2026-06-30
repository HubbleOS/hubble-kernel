/**
 * @file isblank.c
 * @brief Character classification: blank test (space or tab)
 */

#include <hubble/ctype.h>

/**
 * @brief Check if character is blank (space or tab)
 * @param c Character to test
 * @return Non-zero if blank, 0 otherwise
 */
int isblank(int c) { return c == ' ' || c == '\t'; }
