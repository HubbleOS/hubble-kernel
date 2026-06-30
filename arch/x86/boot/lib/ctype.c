/**
 * @file ctype.c
 * @brief Character type classification and conversion implementation
 */

#include "ctype.h"

/** @brief Check if character is alphanumeric */
int isalnum(int c) { return isalpha(c) || isdigit(c); }

/** @brief Check if character is alphabetic */
int isalpha(int c) { return ((unsigned)c | 32) - 'a' < 26; }

/** @brief Check if character is a blank (' ' or '\t') */
int isblank(int c) { return c == ' ' || c == '\t'; }

/** @brief Check if character is a control character */
int iscntrl(int c) { return (unsigned)c < 0x20 || c == 0x7f; }

/** @brief Check if character is a decimal digit */
int isdigit(int c) { return (unsigned)c - '0' < 10; }

/** @brief Check if character has graphical representation (excluding space) */
int isgraph(int c) { return (unsigned)c - 0x21 < 0x5e; }

/** @brief Check if character is a lowercase letter */
int islower(int c) { return (unsigned)c - 'a' < 26; }

/** @brief Check if character is printable (including space) */
int isprint(int c) { return (unsigned)c - 0x20 < 0x5f; }

/** @brief Check if character is punctuation */
int ispunct(int c) { return isgraph(c) && !isalnum(c); }

/** @brief Check if character is whitespace */
int isspace(int c) { return c == ' ' || (unsigned)c - '\t' < 5; }

/** @brief Check if character is an uppercase letter */
int isupper(int c) { return (unsigned)c - 'A' < 26; }

/** @brief Check if character is a hexadecimal digit */
int isxdigit(int c) { return isdigit(c) || ((unsigned)c | 32) - 'a' < 6; }

/** @brief Convert character to lowercase */
int tolower(int c) { return isupper(c) ? c | 0x20 : c; }

/** @brief Convert character to uppercase */
int toupper(int c) { return islower(c) ? c & 0x5f : c; }
