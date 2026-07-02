/**
 * @file tolower.c
 * @brief Character conversion: convert to lowercase
 */

#include <hubble/ctype.h>

/**
 * @brief Convert uppercase letter to lowercase
 * @param c Character to convert
 * @return Lowercase equivalent if uppercase, otherwise c unchanged
 */
int tolower(int c) { return isupper(c) ? c | 0x20 : c; }
