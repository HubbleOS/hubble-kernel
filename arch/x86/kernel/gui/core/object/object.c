#include "object.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../compositor/compositor.h"

#define MAX_ELEMENTS 16

object_t *object_create(int x, int y, int w, int h, color_t bg_color)
{
	object_t *obj = malloc(sizeof(object_t));

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
	uint32_t *obj_buf = obj->buffer;

	for (int i = 0; i < obj->width * obj->height; i++)
		obj->buffer[i] = obj->bg_color;

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

		uint32_t *el_buf = el->buffer;
		int copy_width = end_x - start_x;

		for (int y = start_y; y < end_y; y++)
		{
			uint32_t *obj_row = obj_buf + y * obj->width + start_x;
			uint32_t *el_row = el_buf + (y - start_y) * el->width;

			memcpy(obj_row, el_row, copy_width * sizeof(uint32_t));
		}
	}
}

void object_add_element(object_t *obj, element_t *el)
{
	if (obj->element_count >= MAX_ELEMENTS)
		return;
	obj->elements[obj->element_count++] = el;

	object_redraw_elements(obj);

	compositor_add_damage(obj->layer, obj->x + el->x, obj->y + el->y, el->width, el->height);
}

void object_move_element(object_t *obj, element_t *el, int new_x, int new_y)
{
	if (!obj || !el)
		return;

	rect_t old_rect = {el->x, el->y, el->width, el->height};
	rect_t new_rect = {new_x, new_y, el->width, el->height};
	rect_t dirty_rect = rect_union(old_rect, new_rect);

	el->x = new_x;
	el->y = new_y;

	object_redraw_elements(obj); // сам заливает bg_color и рисует все элементы

	compositor_add_damage(obj->layer,
			      obj->x + dirty_rect.x,
			      obj->y + dirty_rect.y,
			      dirty_rect.w, dirty_rect.h);
}
