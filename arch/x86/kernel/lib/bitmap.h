#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

static inline bool bitmap_test(void *bitmap, size_t bit)
{
	uint8_t *bitmap_u8 = bitmap;
	return bitmap_u8[bit / 8] & (1 << (bit % 8));
}

static inline void bitmap_set(void *bitmap, size_t bit)
{
	uint8_t *bitmap_u8 = bitmap;
	bitmap_u8[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_reset(void *bitmap, size_t bit)
{
	uint8_t *bitmap_u8 = bitmap;
	bitmap_u8[bit / 8] &= ~(1 << (bit % 8));
}
