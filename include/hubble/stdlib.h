#pragma once

/**
 * @brief Number conversion (kernel-internal; no allocation here, see
 *        mm/kmalloc.h).
 */

#include <_cheader.h>

_Begin_C_Header;

/** @brief Decimal string to int: leading spaces, an optional sign, then
 *         digits up to the first non-digit. */
int atoi(const char *s);

/** @brief String to long in @p base (2-36, or 0: 0x prefix hex, 0 prefix
 *         octal, else decimal). @p end, if not NULL, gets where it
 *         stopped. Overflow is not detected. */
long strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);

_End_C_Header;
