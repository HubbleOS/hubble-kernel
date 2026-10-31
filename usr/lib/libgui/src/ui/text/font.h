/**
 * @file font.h
 * @brief Text at any size: TrueType faces rendered with stb_truetype
 *
 *   font_t *f = font_get(FONT_SANS, 14);
 *   int w = font_text_width(f, "Hello");
 *   font_draw(f, buf, buf_w, buf_h, x, y + font_ascent(f), "Hello", color);
 *
 * The faces are read from FONT_DIR on first use, and each size keeps the
 * glyphs it has drawn. Without the font files, text falls back to the
 * built-in 8x8 bitmap font (fonts/font.h), doubled from 16 px up.
 * Text is UTF-8; what the face lacks is drawn as '?'.
 */
#pragma once

#include <stdint.h>

#include <utils/color/color.h>

#define FONT_DIR "/usr/share/fonts"

#define FONT_SIZE_DEFAULT 14 /* pixels from the top of the tallest letter
                              * to the bottom of the deepest */

typedef enum {
  FONT_SANS, /* DejaVu Sans: the interface */
  FONT_MONO, /* DejaVu Sans Mono: terminals, logs */
  FONT_FACES
} font_face_t;

typedef struct font font_t;

/** @p face at @p px pixels (clamped to 6..96). Never NULL. */
font_t *font_get(font_face_t face, int px);

/** Pixels above the baseline, below it, and from one line to the next. */
int font_ascent(const font_t *f);
int font_descent(const font_t *f);
int font_line_height(const font_t *f);

/** Width of @p text drawn with @p f. */
int font_text_width(font_t *f, const char *text);

/** One character's advance; for FONT_MONO, the cell width. */
int font_advance(font_t *f, uint32_t codepoint);

/**
 * Draw @p text into @p buf (@p buf_w by @p buf_h ARGB pixels) starting at
 * @p x with its baseline at @p baseline, blended over what is there and
 * clipped to the buffer. Returns the x after the text.
 */
int font_draw(font_t *f, uint32_t *buf, int buf_w, int buf_h, int x,
              int baseline, const char *text, color_t color);

/** The same for one character. */
int font_draw_char(font_t *f, uint32_t *buf, int buf_w, int buf_h, int x,
                   int baseline, uint32_t codepoint, color_t color);
