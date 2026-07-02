/**
 * @file stdlib.h
 * @brief General utility functions
 */

#pragma once

#include <_cheader.h>
#include <stddef.h>

_Begin_C_Header;

/** @brief Convert string to integer */
int atoi(const char *);

/** @brief Allocate memory */
void *malloc(size_t);

/** @brief Resize memory allocation */
void *realloc(void *ptr, size_t size);

/** @brief Free allocated memory */
void free(void *);

/** @brief Compute absolute value of an int */
int abs(int);

/** @brief Compute absolute value of a long */
long labs(long);

/** @brief Compute absolute value of a long long */
long long llabs(long long x);

_End_C_Header;
