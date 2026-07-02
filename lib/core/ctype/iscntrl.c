/**
 * @file iscntrl.c
 * @brief Character classification: control character test
 */

#include <hubble/ctype.h>

/**
 * @brief Check if character is a control character
 * @param c Character to test
 * @return Non-zero if control character, 0 otherwise
 */
int iscntrl(int c) { return (unsigned)c < 0x20 || c == 0x7f; }
