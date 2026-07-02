#pragma once

/**
 * @brief Colour type and helpers for the kernel console and UI.
 */

#include <stdint.h>

typedef uint32_t color_t; /**< 32-bit ARGB colour value. */
typedef uint8_t chan_t;   /**< Single colour channel (R, G, or B). */
typedef float alpha_t;    /**< Opacity value in [0.0, 1.0]. */

enum Color {
  COLOR_BLACK = 0xFF000000,
  COLOR_WHITE = 0xFFFFFFFF,
  COLOR_RED = 0xFFFF0000,
  COLOR_GREEN = 0xFF00FF00,
  COLOR_BLUE = 0xFF0000FF,
  COLOR_YELLOW = 0xFFFFFF00,
};

#define rgb(r, g, b)                                                           \
  ((color_t)(255 << 24) | ((color_t)(r) << 16) | ((color_t)(g) << 8) |         \
   (color_t)(b))

#define rgba(r, g, b, a)                                                       \
  ((color_t)((uint8_t)((a) * 255.0f + 0.5f) << 24) | ((color_t)(r) << 16) |    \
   ((color_t)(g) << 8) | (color_t)(b))

#define get_alpha(c) (((c) >> 24) & 0xFF)
#define get_red(c) (((c) >> 16) & 0xFF)
#define get_green(c) (((c) >> 8) & 0xFF)
#define get_blue(c) ((c) & 0xFF)

#define make_color(a, r, g, b)                                                 \
  (((color_t)(a) << 24) | ((color_t)(r) << 16) | ((color_t)(g) << 8) |         \
   (color_t)(b))

color_t color_blend(color_t src, color_t dst);
