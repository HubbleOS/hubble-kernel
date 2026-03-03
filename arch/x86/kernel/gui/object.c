#include "object.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "compositor.h"

#define MAX_ELEMENTS 16

object_t *object_create(int x, int y, int w, int h, color_t bg_color)
{
	object_t *obj = malloc(sizeof(object_t));
	// object_t *obj = calloc(1, sizeof(object_t));

	if (!obj)
		return NULL;

	obj->x = x;
	obj->y = y;
	obj->width = w;
	obj->height = h;
	obj->bg_color = bg_color;

	obj->elements = malloc(MAX_ELEMENTS * sizeof(element_t *));
	obj->element_count = 0;
	obj->element_capacity = MAX_ELEMENTS;

	obj->buffer = malloc(w * h * sizeof(uint32_t));

	if (!obj->buffer)
	{
		free(obj);
		return NULL;
	}

	memset(obj->buffer, 0, w * h * sizeof(uint32_t));

	for (size_t i = 0; i < w * h; i++)
		obj->buffer[i] = bg_color;

	return obj;
}

void object_destroy(object_t *obj)
{
	free(obj->buffer);
	free(obj);
}

void object_redraw_elements(object_t *obj)
{
	for (int e = 0; e < obj->element_count; e++)
	{
		element_t *el = obj->elements[e];

		int start_x = el->x;
		int start_y = el->y;
		int end_x = start_x + el->width;
		int end_y = start_y + el->height;

		if (end_x > obj->width)
			end_x = obj->width;
		if (end_y > obj->height)
			end_y = obj->height;

		for (int y = start_y; y < end_y; y++)
		{
			for (int x = start_x; x < end_x; x++)
			{
				int el_px = x - el->x;
				int el_py = y - el->y;

				uint32_t src = el->buffer[el_py * el->width + el_px];
				obj->buffer[y * obj->width + x] = color_blend(src, obj->buffer[y * obj->width + x]);
			}
		}
	}
}

void object_add_element(object_t *obj, element_t *el)
{
	if (obj->element_count >= MAX_ELEMENTS)
		return;
	obj->elements[obj->element_count++] = el;

	// сразу отрисовать элемент на buffer
	object_redraw_elements(obj);

	// пометить область как dirty
	compositor_add_damage(obj->x + el->x, obj->y + el->y, el->width, el->height);
}
