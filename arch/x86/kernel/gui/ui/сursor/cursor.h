#pragma once

#include <gui/core/object/object.h>
#include <gui/core/compositor/compositor.h>

typedef struct
{
	object_t *surface; // cursor object
	int width, height;
	int x, y; // screen position
} cursor_t;

cursor_t *cursor_create(int w, int h, uint32_t color_outer, uint32_t color_inner);
void cursor_destroy(cursor_t *c);
void cursor_move(cursor_t *c, int x, int y);
