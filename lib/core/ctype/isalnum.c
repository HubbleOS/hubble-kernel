/**
 * @file isalnum.c
 * @brief Character classification: alphanumeric test
 */

#include <hubble/ctype.h>

/**
 * @brief Check if character is alphanumeric
 * @param c Character to test
 * @return Non-zero if alphanumeric, 0 otherwise
 */
int isalnum(int c) { return isalpha(c) || isdigit(c); }
