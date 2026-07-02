/**
 * @file isprint.c
 * @brief Character classification: printable (including space) test
 */

#include <hubble/ctype.h>

/**
 * @brief Check if character is printable (including space)
 * @param c Character to test
 * @return Non-zero if printable, 0 otherwise
 */
int isprint(int c) { return (unsigned)c - 0x20 < 0x5f; }
