#pragma once

#include <core/object/object.h>
#include <core/compositor/compositor.h>

typedef struct
{
	object_t *surface; // cursor object
} cursor_t;

cursor_t *cursor_create(int w, int h, uint32_t color_outer, uint32_t color_inner);
void cursor_destroy(cursor_t *c);
void cursor_move(cursor_t *c, int x, int y);
