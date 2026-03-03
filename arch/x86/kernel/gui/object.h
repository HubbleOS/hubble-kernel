#pragma once

#include <stdint.h>
#include "utils/color.h"
#include "ui/element.h"

typedef struct
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
