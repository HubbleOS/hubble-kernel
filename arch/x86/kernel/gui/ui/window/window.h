#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifndef GUI_DEMO
#include <bootinfo/framebuffer.h>
#else
#include "platform/fb.h"
#endif
#include <gui/utils/color/color.h>
#include <gui/core/object/object.h>

typedef struct
{
	object_t *surface;

	bool visible;
	bool focused;

	char title[64];
} window_t;

window_t *window_create(int x, int y, int w, int h);
void window_destroy(window_t *win);

int window_addElement(window_t *win, element_t *el);

void window_move(window_t *win, int x, int y);
void window_resize(window_t *win, int w, int h);
void window_focus(window_t *win);
