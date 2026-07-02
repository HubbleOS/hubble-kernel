/**
 * @file string.h
 * @brief String and memory manipulation declarations
 */

#pragma once

#include <_cheader.h>
#include <stddef.h>
#include <stdint.h>

_Begin_C_Header;

/** @brief Fill memory with a constant byte */
void *memset(void *, int, size_t);

/** @brief Copy memory region */
void *memcpy(void *, const void *, size_t);

/** @brief Copy memory region (may overlap) */
void *memmove(void *, const void *, size_t);

/** @brief Compare memory regions */
int memcmp(const void *, const void *, size_t);

/** @brief Scan memory for a byte */
void *memchr(const void *, int, size_t);

/** @brief Copy a string */
char *strcpy(char *, const char *);

/** @brief Copy a string with length limit */
char *strncpy(char *, const char *, size_t);

/** @brief Concatenate strings */
char *strcat(char *, const char *);

/** @brief Concatenate strings with length limit */
char *strncat(char *, const char *, size_t);

/** @brief Duplicate a string */
char *strdup(const char *);

/** @brief Transform a string for locale-aware comparison */
size_t strxfrm(char *, const char *, size_t);

/** @brief Get string length */
size_t strlen(const char *);

/** @brief Compare two strings */
int strcmp(const char *, const char *);

/** @brief Compare two strings with length limit */
int strncmp(const char *, const char *, size_t);

/** @brief Locale-aware string comparison */
int strcoll(const char *, const char *);

/** @brief Find a character in a string */
char *strchr(const char *, int);

/** @brief Find a character in a string (reverse) */
char *strrchr(const char *, int);

/** @brief Get bounded string length */
size_t strnlen(const char *, size_t);

_End_C_Header;
