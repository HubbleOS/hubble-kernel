#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <bootinfo/framebuffer.h>

#include "utils/color.h"

#include "object.h"

typedef struct
{
	object_t *surface;

	bool visible;
	bool focused;

	char title[64];

} window_t;

int window_drawPixel(window_t *win, int x, int y, color_t color);
int window_drawRect(window_t *win, int x, int y, int w, int h, color_t color);
window_t *window_create(int x, int y, int w, int h);
void window_destroy(window_t *win);

void window_move(window_t *win, int x, int y);
void window_focus(window_t *win);
