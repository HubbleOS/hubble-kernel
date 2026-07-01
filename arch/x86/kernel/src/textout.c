/**
 * @file textout.c
 * @brief Framebuffer text output — glyph rendering
 *
 * Looks up bitmap glyphs from the built-in font and draws them
 * onto the linear framebuffer with per-pixel alpha blending.
 */

#include "textout.h"
#include <hubble/color.h>
#include <hubble/font.h>
#include <hubble/platform.h>

/**
 * @brief Return the 8x8 glyph bitmap for a given character
 *
 * @param c Character code (0-127)
 * @return Pointer to the 8-byte glyph bitmap, or NULL
 */
static uint8_t *get_glyph(char c) {
  if ((unsigned char)c >= 128)
    return 0;
  return font[(unsigned char)c];
}

/**
 * @brief Draw a monochrome glyph onto the framebuffer with scaling
 *
 * @param glyph     8-byte glyph bitmap
 * @param pitch     Framebuffer pitch in pixels
 * @param x         Screen X position
 * @param y         Screen Y position
 * @param w         Glyph width in pixels
 * @param h         Glyph height in pixels
 * @param scale_x   Horizontal scale factor
 * @param scale_y   Vertical scale factor
 * @param font_color Colour for foreground pixels
 */
static void draw_pixel_array_scaled(uint8_t *glyph, int pitch, int x, int y,
                                    int w, int h, int scale_x, int scale_y,
                                    color_t font_color) {
  for (int row = 0; row < h; ++row) {
    uint8_t line = glyph[row];

    for (int col = 0; col < w; ++col) {
      if (line & (0x80 >> col))
        for (int dy = 0; dy < scale_y; ++dy)
          for (int dx = 0; dx < scale_x; ++dx) {
            unsigned int px = (unsigned int)(x + col * scale_x + dx);
            unsigned int py = (unsigned int)(y + row * scale_y + dy);

            if (px < g_platform.fb_width && py < g_platform.fb_height) {
              color_t *pixel =
                  &((uint32_t *)g_platform.fb_base)[py * pitch + px];
              color_t dst_color = *pixel;
              color_t blended = color_blend(font_color, dst_color);
              *pixel = blended;
            }
          }
    }
  }
}

/**
 * @brief Draw a single character on the framebuffer
 *
 * @param c           Character to draw
 * @param x           Screen X position
 * @param y           Screen Y position
 * @param w           Character width
 * @param h           Character height
 * @param font_color  Colour for the character
 */
void draw_char(char c, int x, int y, int w, int h, color_t font_color) {
  uint8_t *glyph = get_glyph(c);
  if (!glyph)
    glyph = get_glyph('!');

  int pitch = g_platform.fb_pitch / 4;
  draw_pixel_array_scaled(glyph, pitch, x, y, w, h, 1, 1, font_color);
}
