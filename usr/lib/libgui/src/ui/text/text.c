#include "text.h"
#include "font.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <utils/color/color.h>

static void draw_text_in_element(element_t *el) {
  if (!el || !el->text || !el->buffer)
    return;

  for (int i = 0; i < el->width * el->height; i++)
    el->buffer[i] = 0; /* transparent: the text over what is below */

  font_t *f = element_font(el);
  font_draw(f, el->buffer, el->width, el->height, 0, font_ascent(f), el->text,
            el->text_color);
}

/* A label exactly the size of @p text, in the default font. */
text_t *element_create_text(int x, int y, const char *text) {
  if (!text)
    return NULL;

  font_t *f = font_get(FONT_SANS, FONT_SIZE_DEFAULT);
  int w = font_text_width(f, text);
  text_t *el = element_create(x, y, w > 0 ? w : 1, font_line_height(f));
  if (!el)
    return NULL;

  el->type = UI_LABEL;
  el->draw = draw_text_in_element;
  el->text_color = rgb(255, 255, 255);

  el->text = strdup(text);
  if (!el->text) {
    element_destroy((element_t *)el);
    return NULL;
  }

  el->dirty_rect = (dirty_rect_t){0, 0, el->width, el->height, true};

  draw_text_in_element(el);

  return el;
}

void text_destroy(element_t *el) { element_destroy(el); }
