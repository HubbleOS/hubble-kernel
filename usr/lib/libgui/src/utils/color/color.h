#pragma once

#include <stdint.h>

typedef uint32_t color_t; // 32-bit color value (ARGB)
typedef uint8_t chan_t;   // color channel (R, G, or B)
typedef float alpha_t;    // opacity / alpha value (0.0–1.0)

enum Color {
  COLOR_BLACK = 0xFF000000,
  COLOR_WHITE = 0xFFFFFFFF,
  COLOR_RED = 0xFFFF0000,
  COLOR_GREEN = 0xFF00FF00,
  COLOR_BLUE = 0xFF0000FF,
  COLOR_YELLOW = 0xFFFFFF00
};

#define rgb(r, g, b)                                                           \
  ((color_t)b << 0) | ((color_t)g << 8) | ((color_t)r << 16) |                 \
      ((color_t)255 << 24)
// #define rgba(r, g, b, a) ((color_t)b << 0) | ((color_t)g << 8) | ((color_t)r
// << 16) | ((color_t)a << 24)
#define rgba(r, g, b, a)                                                       \
  ((color_t)b << 0) | ((color_t)g << 8) | ((color_t)r << 16) |                 \
      ((uint32_t)(a * 255.0f + 0.5f) << 24)

#define get_alpha(c) ((c >> 24) & 0xFF)
#define get_red(c) ((c >> 16) & 0xFF)
#define get_green(c) ((c >> 8) & 0xFF)
#define get_blue(c) (c & 0xFF)

#define make_color(a, r, g, b)                                                 \
  ((color_t)b << 0) | ((color_t)g << 8) | ((color_t)r << 16) |                 \
      ((color_t)a << 24)

color_t color_blend(color_t src, color_t dst);
