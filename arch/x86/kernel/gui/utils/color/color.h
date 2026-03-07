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

static inline color_t rgb(chan_t r, chan_t g, chan_t b)
{
	return ((color_t)b << 0) | ((color_t)g << 8) | ((color_t)r << 16) | ((color_t)255 << 24);
}

static inline color_t rgba(chan_t r, chan_t g, chan_t b, float a)
{
	return ((color_t)b << 0) | ((color_t)g << 8) | ((color_t)r << 16) | ((uint32_t)(a * 255.0f + 0.5f) << 24);
}

static inline uint8_t get_alpha(color_t c) { return (c >> 24) & 0xFF; }
static inline uint8_t get_red(color_t c) { return (c >> 16) & 0xFF; }
static inline uint8_t get_green(color_t c) { return (c >> 8) & 0xFF; }
static inline uint8_t get_blue(color_t c) { return c & 0xFF; }

static inline color_t make_color(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
	return ((color_t)a << 24) | ((color_t)r << 16) | ((color_t)g << 8) | b;
}

color_t color_blend(color_t src, color_t dst);
