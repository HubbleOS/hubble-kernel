/**
 * @file toupper.c
 * @brief Character conversion: convert to uppercase
 */

#include <hubble/ctype.h>

/**
 * @brief Convert lowercase letter to uppercase
 * @param c Character to convert
 * @return Uppercase equivalent if lowercase, otherwise c unchanged
 */
int toupper(int c) { return islower(c) ? c & 0x5f : c; }
