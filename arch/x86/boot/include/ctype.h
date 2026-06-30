/**
 * @file ctype.h
 * @brief Character type classification and conversion functions for the bootloader
 */

#pragma once

/** @brief Check if character is alphanumeric */
int isalnum(int);

/** @brief Check if character is alphabetic */
int isalpha(int);

/** @brief Check if character is a blank (' ' or '\t') */
int isblank(int);

/** @brief Check if character is a control character */
int iscntrl(int);

/** @brief Check if character is a decimal digit */
int isdigit(int);

/** @brief Check if character has graphical representation (excluding space) */
int isgraph(int);

/** @brief Check if character is a lowercase letter */
int islower(int);

/** @brief Check if character is printable (including space) */
int isprint(int);

/** @brief Check if character is punctuation */
int ispunct(int);

/** @brief Check if character is whitespace */
int isspace(int);

/** @brief Check if character is an uppercase letter */
int isupper(int);

/** @brief Check if character is a hexadecimal digit */
int isxdigit(int);

/** @brief Convert character to lowercase */
int tolower(int);

/** @brief Convert character to uppercase */
int toupper(int);
