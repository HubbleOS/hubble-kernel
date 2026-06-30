/**
 * @file bitmap.h
 * @brief Simple bit array manipulation helpers
 *
 * Provides inline functions for testing, setting, and clearing
 * individual bits in a bitmap backed by a byte array.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Test whether a bit is set
 *
 * @param bitmap Pointer to the bitmap byte array
 * @param bit    Index of the bit to test
 * @return true if the bit is set, false otherwise
 */
static inline bool bitmap_test(void *bitmap, size_t bit)
{
	uint8_t *bitmap_u8 = bitmap;
	return bitmap_u8[bit / 8] & (1 << (bit % 8));
}

/**
 * @brief Set a bit to 1
 *
 * @param bitmap Pointer to the bitmap byte array
 * @param bit    Index of the bit to set
 */
static inline void bitmap_set(void *bitmap, size_t bit)
{
	uint8_t *bitmap_u8 = bitmap;
	bitmap_u8[bit / 8] |= (1 << (bit % 8));
}

/**
 * @brief Clear a bit to 0
 *
 * @param bitmap Pointer to the bitmap byte array
 * @param bit    Index of the bit to clear
 */
static inline void bitmap_clear(void *bitmap, size_t bit)
{
	uint8_t *bitmap_u8 = bitmap;
	bitmap_u8[bit / 8] &= ~(1 << (bit % 8));
}
