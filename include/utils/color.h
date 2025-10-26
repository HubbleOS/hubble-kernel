#pragma once

#include <stdint.h>

typedef uint32_t color_t; // 32-bit color value (ARGB)
typedef uint8_t chan_t;	  // color channel (R, G, or B)
typedef float alpha_t;	  // opacity / alpha value (0.0–1.0)

enum Color
{
	COLOR_BLACK = 0x000000,
	COLOR_WHITE = 0xFFFFFF,
	COLOR_RED = 0xFF0000,
	COLOR_GREEN = 0x00FF00,
	COLOR_BLUE = 0x0000FF,
	COLOR_YELLOW = 0xFFFF00
};

inline color_t rgb(chan_t r, chan_t g, chan_t b) { return ((color_t)r << 16) | ((color_t)g << 8) | b; }
inline color_t rgba(chan_t r, chan_t g, chan_t b, alpha_t a) { return rgb(r, g, b) | ((color_t)(a * 255.0f + 0.5f) << 24); }
