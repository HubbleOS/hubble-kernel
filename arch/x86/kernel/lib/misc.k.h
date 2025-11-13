/**
 * @file misk.k.h
 * @brief Miscellaneous utility macros for kernel development
 *
 * @note This header is intended for internal kernel usage and should
 *       not be used in standard user-space code.
 */

#pragma once

/**
 * @brief Return the smaller of two values
 *
 * @param A First value
 * @param B Second value
 * @return The lesser of @p A and @p B
 *
 * This macro safely evaluates each argument exactly once using `__auto_type`
 * to prevent double evaluation or side effects.
 *
 * Example:
 *   int x = 5, y = 10;
 *   int m = MIN(x++, y); // m = 5, x becomes 6
 */
#define MIN(A, B) ({                   \
	__auto_type MIN_a = A;         \
	__auto_type MIN_b = B;         \
	MIN_a < MIN_b ? MIN_a : MIN_b; \
})

/**
 * @brief Return the larger of two values
 *
 * @param A First value
 * @param B Second value
 * @return The greater of @p A and @p B
 *
 * Similar to MIN(), this macro evaluates its arguments only once
 * and works safely with expressions containing side effects.
 *
 * Example:
 *   int a = 3, b = 7;
 *   int max_val = MAX(a, b); // 7
 */
#define MAX(A, B) ({                   \
	__auto_type MAX_a = A;         \
	__auto_type MAX_b = B;         \
	MAX_a > MAX_b ? MAX_a : MAX_b; \
})

/**
 * @brief Divide and round up the result
 *
 * @param VALUE The value to divide
 * @param DIV The divisor
 * @return The smallest integer ≥ VALUE / DIV
 *
 * This macro ensures that partial divisions are rounded up.
 * It’s often used to compute the number of pages, blocks, or chunks
 * required to store a given size.
 *
 * Example:
 *   size_t pages = DIV_ROUNDUP(5000, 4096); // → 2
 */
#define DIV_ROUNDUP(VALUE, DIV) ({                                     \
	__auto_type DIV_ROUNDUP_value = VALUE;                         \
	__auto_type DIV_ROUNDUP_div = DIV;                             \
	(DIV_ROUNDUP_value + (DIV_ROUNDUP_div - 1)) / DIV_ROUNDUP_div; \
})

/**
 * @brief Align value upwards to the nearest multiple of ALIGN
 *
 * @param VALUE The value to align
 * @param ALIGN Alignment boundary (must be power of two for page alignment)
 * @return The smallest multiple of @p ALIGN greater than or equal to @p VALUE
 *
 * Internally uses DIV_ROUNDUP() to compute the correct multiple.
 * Commonly used to align addresses or sizes to page or sector boundaries.
 *
 * Example:
 *   size_t aligned = ALIGN_UP(5000, 4096); // → 8192
 */
#define ALIGN_UP(VALUE, ALIGN) ({                                     \
	__auto_type ALIGN_UP_value = VALUE;                           \
	__auto_type ALIGN_UP_align = ALIGN;                           \
	DIV_ROUNDUP(ALIGN_UP_value, ALIGN_UP_align) * ALIGN_UP_align; \
})

/**
 * @brief Align value downwards to the nearest multiple of ALIGN
 *
 * @param VALUE The value to align
 * @param ALIGN Alignment boundary (must be nonzero)
 * @return The largest multiple of @p ALIGN less than or equal to @p VALUE
 *
 * This is used to truncate an address or size to a lower alignment boundary,
 * e.g. to get the base page address for a given pointer.
 *
 * Example:
 *   size_t aligned = ALIGN_DOWN(5000, 4096); // → 4096
 */
#define ALIGN_DOWN(VALUE, ALIGN) ({                               \
	__auto_type ALIGN_DOWN_value = VALUE;                     \
	__auto_type ALIGN_DOWN_align = ALIGN;                     \
	(ALIGN_DOWN_value / ALIGN_DOWN_align) * ALIGN_DOWN_align; \
})

/**
 * @brief Get the number of elements in an array
 *
 * @param ARRAY The array variable (not a pointer)
 * @return Number of elements in @p ARRAY
 *
 * Safe and compile-time evaluable. Using it on a pointer produces incorrect
 * results, so always ensure the argument is a real array.
 *
 * Example:
 *   int arr[5];
 *   size_t count = SIZEOF_ARRAY(arr); // → 5
 */
#define SIZEOF_ARRAY(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))
