#pragma once

#include <stdint.h>

typedef uint32_t color;

enum Color
{
	COLOR_BLACK = 0x000000,
	COLOR_WHITE = 0xFFFFFF,
	COLOR_RED = 0xFF0000,
	COLOR_GREEN = 0x00FF00,
	COLOR_BLUE = 0x0000FF,
	COLOR_YELLOW = 0xFFFF00
};

inline color rgb(uint8_t r, uint8_t g, uint8_t b) { return ((color)r << 16) | ((color)g << 8) | b; }

inline color rgba(uint8_t r, uint8_t g, uint8_t b, float a)
{
	uint8_t alpha = (uint8_t)(a * 255.0f + 0.5f);
	return rgb(r, g, b) | ((color)alpha << 24);
}
