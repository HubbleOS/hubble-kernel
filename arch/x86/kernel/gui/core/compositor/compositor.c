#include "compositor.h"
#include "../screen/screen.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

compositor_t compositor;

void compositor_add_damage(int x, int y, int w, int h)
{
	if (compositor.dirty_count >= MAX_DIRTY || w <= 0 || h <= 0)
		return;

	// clamp
	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (y < 0)
	{
		h += y;
		y = 0;
	}
	if (x + w > fb_width)
		w = fb_width - x;
	if (y + h > fb_height)
		h = fb_height - y;
	if (w <= 0 || h <= 0)
		return;

	compositor.dirty[compositor.dirty_count++] = (rect_t){x, y, w, h};
}

void compositor_init()
{
	for (int i = 0; i < MAX_LAYERS; i++)
		compositor.layers[i].count = 0;
	compositor.dirty_count = 0;
}

void compositor_add(object_t *obj, int layer)
{
	if (layer < 0 || layer >= MAX_LAYERS)
		return;

	layer_t *l = &compositor.layers[layer];
	if (l->count >= MAX_OBJECTS)
		return;

	l->objects[l->count++] = obj;
	compositor_add_damage(obj->x, obj->y, obj->width, obj->height);
}

void compositor_remove(object_t *obj, int layer)
{
	if (layer < 0 || layer >= MAX_LAYERS)
		return;

	layer_t *l = &compositor.layers[layer];
	for (int i = 0; i < l->count; i++)
	{
		if (l->objects[i] == obj)
		{
			compositor_add_damage(obj->x, obj->y, obj->width, obj->height);
			l->objects[i] = l->objects[l->count - 1];
			l->count--;
			return;
		}
	}
}

void compositor_render()
{
	merge_dirty_rects(compositor.dirty, &compositor.dirty_count);

	for (int d = 0; d < compositor.dirty_count; d++)
	{
		rect_t r = compositor.dirty[d];

		// clear dirty region
		for (int row = r.y; row < r.y + r.h; row++)
			memset(framebuffer_back + row * fb_width + r.x, 0, r.w * sizeof(uint32_t));

		// render layers bottom → top
		for (int l = 0; l < MAX_LAYERS; l++)
		{
			layer_t *layer = &compositor.layers[l];
			for (int i = 0; i < layer->count; i++)
			{
				object_t *obj = layer->objects[i];

				int obj_x1 = obj->x, obj_y1 = obj->y;
				int obj_x2 = obj->x + obj->width;
				int obj_y2 = obj->y + obj->height;

				int r_x2 = r.x + r.w, r_y2 = r.y + r.h;

				if (obj_x1 >= r_x2 || obj_x2 <= r.x || obj_y1 >= r_y2 || obj_y2 <= r.y)
					continue;

				int start_x = obj_x1 > r.x ? obj_x1 : r.x;
				int start_y = obj_y1 > r.y ? obj_y1 : r.y;
				int end_x = obj_x2 < r_x2 ? obj_x2 : r_x2;
				int end_y = obj_y2 < r_y2 ? obj_y2 : r_y2;

				for (int y = start_y; y < end_y; y++)
				{
					for (int x = start_x; x < end_x; x++)
					{
						int obj_px = x - obj->x;
						int obj_py = y - obj->y;
						uint32_t src = obj->buffer[obj_py * obj->width + obj_px];
						uint32_t *dst = framebuffer_back + y * fb_width + x;
						*dst = color_blend(src, *dst);
					}
				}
			}
		}

		screen_present_rect(r.x, r.y, r.w, r.h);
	}

	compositor.dirty_count = 0;
}

void compositor_bring_to_front(object_t *obj, int layer)
{
	if (layer < 0 || layer >= MAX_LAYERS)
		return;

	layer_t *l = &compositor.layers[layer];
	int index = -1;

	for (int i = 0; i < l->count; i++)
		if (l->objects[i] == obj)
		{
			index = i;
			break;
		}

	if (index == -1 || index == l->count - 1)
		return;

	for (int i = index; i < l->count - 1; i++)
		l->objects[i] = l->objects[i + 1];

	l->objects[l->count - 1] = obj;
}

void compositor_move_object(object_t *obj, int new_x, int new_y)
{
	rect_t old_rect = {obj->x, obj->y, obj->width, obj->height};
	rect_t new_rect = {new_x, new_y, obj->width, obj->height};
	rect_t rect = rect_union(old_rect, new_rect);

	compositor_add_damage(rect.x, rect.y, rect.w, rect.h);

	obj->x = new_x;
	obj->y = new_y;
}

void compositor_change_size_object(object_t *obj, int new_w, int new_h)
{
	rect_t old_rect = {obj->x, obj->y, obj->width, obj->height};
	rect_t new_rect = {obj->x, obj->y, new_w, new_h};
	rect_t rect = rect_union(old_rect, new_rect);

	compositor_add_damage(rect.x, rect.y, rect.w, rect.h);

	uint32_t *new_buf = malloc(new_w * new_h * sizeof(uint32_t));
	if (!new_buf)
		return;
	memset(new_buf, 0, new_w * new_h * sizeof(uint32_t));

	int copy_w = obj->width < new_w ? obj->width : new_w;
	int copy_h = obj->height < new_h ? obj->height : new_h;
	for (int y = 0; y < copy_h; y++)
		memcpy(new_buf + y * new_w, obj->buffer + y * obj->width, copy_w * sizeof(uint32_t));

	free(obj->buffer);
	obj->buffer = new_buf;
	obj->width = new_w;
	obj->height = new_h;

	object_redraw_elements(obj);
}
