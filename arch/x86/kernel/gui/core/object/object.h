#pragma once

#include <stdint.h>
#include <gui/utils/color/color.h>
#include <gui/ui/element.h>

typedef struct object
{
	int x, y;
	int width, height;

	uint32_t *buffer;
	color_t bg_color;

	element_t **elements;
	int element_count;
	int element_capacity;
} object_t;

object_t *object_create(int x, int y, int w, int h, color_t bg_color);
void object_destroy(object_t *obj);

void object_redraw_elements(object_t *obj);
void object_add_element(object_t *obj, element_t *el);
void object_move_element(object_t *obj, element_t *el, int new_x, int new_y);
