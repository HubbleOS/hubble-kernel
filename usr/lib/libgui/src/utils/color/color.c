#include "color.h"

color_t color_blend(color_t src, color_t dst) {
  uint8_t alpha = get_alpha(src);

  if (alpha == 0)
    return dst;
  if (alpha == 255)
    return src;

  uint8_t inv_alpha = 255 - alpha;

  uint8_t r = (get_red(src) * alpha + get_red(dst) * inv_alpha) / 255;
  uint8_t g = (get_green(src) * alpha + get_green(dst) * inv_alpha) / 255;
  uint8_t b = (get_blue(src) * alpha + get_blue(dst) * inv_alpha) / 255;

  return make_color(255, r, g, b);
}
