#pragma once

#include <core/element/element.h>

#include <stdint.h>
#include <stddef.h>

typedef element_t canvas_t;

canvas_t *canvas_create(int x, int y, int width, int height);

void canvas_destroy(canvas_t *c);

void canvas_clear(canvas_t *c, uint32_t color);

void canvas_set_pixel(canvas_t *c, int x, int y, uint32_t color);

void canvas_draw_line(canvas_t *c, int x1, int y1, int x2, int y2, uint32_t color);

void canvas_draw_rect(canvas_t *c, int x, int y, int w, int h, uint32_t color);

void canvas_draw_text(canvas_t *c, int x, int y, const char *text, uint32_t color);
