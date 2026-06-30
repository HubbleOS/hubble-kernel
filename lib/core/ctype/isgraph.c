/**
 * @file isgraph.c
 * @brief Character classification: printable (non-space) test
 */

#include <hubble/ctype.h>

/**
 * @brief Check if character is printable (excluding space)
 * @param c Character to test
 * @return Non-zero if printable non-space, 0 otherwise
 */
int isgraph(int c) { return (unsigned)c - 0x21 < 0x5e; }
