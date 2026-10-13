#include "canvas.h"
#include "../text/text.h"
#include <stdlib.h>
#include <string.h>
#include <utils/color/color.h>

#include <core/object/object.h>

canvas_t *canvas_create(int x, int y, int width, int height) {
  element_t *el = element_create(x, y, width, height);
  if (!el)
    return NULL;

  el->type = UI_RECT;

  el->draw = NULL;

  el->on_mouse_enter = NULL;
  el->on_mouse_leave = NULL;
  el->on_mouse_down = NULL;
  el->on_mouse_up = NULL;

  return el;
}

void canvas_destroy(canvas_t *c) { element_destroy(c); }

void canvas_clear(canvas_t *c, uint32_t color) {
  if (!c || !c->buffer)
    return;

  int total = c->width * c->height;
  uint32_t *buf = c->buffer;

  for (int i = 0; i < c->width; i++)
    buf[i] = color;

  for (int row = 1; row < c->height; row++)
    memcpy(buf + row * c->width, buf, c->width * sizeof(uint32_t));

  element_mark_dirty(c);
}

void canvas_set_pixel(canvas_t *c, int x, int y, uint32_t color) {
  if (!c || !c->buffer)
    return;

  if (x < 0 || y < 0 || x >= c->width || y >= c->height)
    return;

  c->buffer[y * c->width + x] = color;
}

void canvas_draw_line(canvas_t *c, int x0, int y0, int x1, int y1,
                      uint32_t color) {
  int dx = abs(x1 - x0);
  int dy = abs(y1 - y0);

  int sx = (x0 < x1) ? 1 : -1;
  int sy = (y0 < y1) ? 1 : -1;

  int err = dx - dy;

  while (1) {
    canvas_set_pixel(c, x0, y0, color);

    if (x0 == x1 && y0 == y1)
      break;

    int e2 = err * 2;

    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }

    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }

  int bx = x0 < x1 ? x0 : x1;
  int by = y0 < y1 ? y0 : y1;
  int bw = abs(x1 - x0) + 1;
  int bh = abs(y1 - y0) + 1;
}

void canvas_draw_rect(canvas_t *c, int x, int y, int w, int h, uint32_t color) {
  if (!c || !c->buffer)
    return;

  for (int row = y; row < y + h && row < c->height; row++) {
    if (row < 0)
      continue;
    int cx = x < 0 ? 0 : x;
    int cw = (x + w > c->width ? c->width - cx : x + w - cx);
    if (cw <= 0)
      continue;

    uint32_t *dst = c->buffer + row * c->width + cx;
    for (int i = 0; i < cw; i++)
      dst[i] = color;
  }
}

void canvas_draw_text(canvas_t *c, int x, int y, const char *text,
                      uint32_t color) {
  if (!c || !text)
    return;

  text_t *txt = element_create_text(0, 0, text);
  if (!txt)
    return;

  for (int row = 0; row < txt->height; row++) {
    for (int col = 0; col < txt->width; col++) {
      uint32_t px = txt->buffer[row * txt->width + col];
      if (px == rgb(255, 255, 255))
        canvas_set_pixel(c, x + col, y + row, color);
    }
  }

  free(txt->buffer);
  free(txt->text);
  free(txt);
}
