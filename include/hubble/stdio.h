#pragma once

/**
 * @brief Formatting into buffers (kernel-internal; printk writes to the
 *        console).
 */

#include <_cheader.h>
#include <stdarg.h>
#include <stddef.h>

_Begin_C_Header;

/**
 * @brief C99 snprintf: %d %i %u %x %X %o %c %s %p %%, with flags - 0 +
 *        and space, a width and precision (or *), and the hh h l ll z t j
 *        length modifiers. No floating point.
 * @return The length the whole output would have; at most @p size - 1
 *         characters and a terminator are written.
 */
int snprintf(char *buf, size_t size, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

_End_C_Header;
