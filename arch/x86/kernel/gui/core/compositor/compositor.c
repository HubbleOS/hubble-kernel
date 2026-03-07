#include "compositor.h"
#include "../screen/screen.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <printk.h>

compositor_t compositor;

void compositor_init()
{
	for (int i = 0; i < MAX_LAYERS; i++)
		compositor.layers[i].count = 0;
	compositor.dirty_count = 0;
}

void compositor_add_damage(int layer, int x, int y, int w, int h)
{
	(void)layer;

	if (compositor.dirty_count >= MAX_DIRTY || w <= 0 || h <= 0)
		return;

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

void compositor_remove(object_t *obj, int layer)
{
	if (layer < 0 || layer >= MAX_LAYERS)
		return;

	layer_t *l = &compositor.layers[layer];
	for (int i = 0; i < l->count; i++)
	{
		if (l->objects[i] == obj)
		{
			compositor_add_damage(layer, obj->x, obj->y, obj->width, obj->height);
			l->objects[i] = l->objects[l->count - 1];
			l->count--;
			return;
		}
	}
}

void compositor_compose(rect_t *dirty, int dirty_count)
{
	for (int d = 0; d < dirty_count; d++)
	{
		rect_t r = dirty[d];

		for (int y = r.y; y < r.y + r.h; y++)
		{
			uint32_t *dst = framebuffer_back + y * fb_width + r.x;
			memset(dst, 0, r.w * sizeof(uint32_t));

			for (int l = 0; l < MAX_LAYERS; l++)
			{
				layer_t *layer = &compositor.layers[l];

				for (int i = 0; i < layer->count; i++)
				{
					object_t *obj = layer->objects[i];

					if (y < obj->y || y >= obj->y + obj->height)
						continue;

					int ix1 = obj->x > r.x ? obj->x : r.x;
					int ix2 = (obj->x + obj->width) < (r.x + r.w)
						      ? (obj->x + obj->width)
						      : (r.x + r.w);
					if (ix1 >= ix2)
						continue;

					uint32_t *src = obj->buffer + (y - obj->y) * obj->width + (ix1 - obj->x);
					uint32_t *d = dst + (ix1 - r.x);

					for (int x = 0; x < ix2 - ix1; x++)
					{
						if ((src[x] >> 24) == 0)
							continue;

						d[x] = color_blend(src[x], d[x]);
					}
				}
			}
		}

		screen_present_rect(r.x, r.y, r.w, r.h);
	}
}

void compositor_frame()
{
	if (compositor.dirty_count == 0)
		return;

	merge_dirty_rects(compositor.dirty, &compositor.dirty_count);

	compositor_compose(compositor.dirty, compositor.dirty_count);
	compositor.dirty_count = 0;
}

void compositor_render()
{
	compositor_frame();
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

	compositor_add_damage(layer, obj->x, obj->y, obj->width, obj->height);
}

void compositor_add(object_t *obj, int layer)
{
	if (layer < 0 || layer >= MAX_LAYERS)
		return;
	layer_t *l = &compositor.layers[layer];
	if (l->count >= MAX_OBJECTS)
		return;

	obj->layer = layer;
	l->objects[l->count++] = obj;

	compositor_add_damage(layer, obj->x, obj->y, obj->width, obj->height);
}

void compositor_move_object(object_t *obj, int new_x, int new_y)
{
	if (obj->layer == LAYER_CURSOR)
	{
		rect_t old_rect = {obj->x, obj->y, obj->width, obj->height};
		rect_t new_rect = {new_x, new_y, obj->width, obj->height};
		rect_t rect = rect_union(old_rect, new_rect);
		compositor_add_damage(obj->layer, rect.x, rect.y, rect.w, rect.h);
	}
	else
	{
		compositor_add_damage(obj->layer, obj->x, obj->y, obj->width, obj->height);
		compositor_add_damage(obj->layer, new_x, new_y, obj->width, obj->height);
	}

	obj->x = new_x;
	obj->y = new_y;
}

void compositor_change_size_object(object_t *obj, int new_w, int new_h)
{
	rect_t old_rect = {obj->x, obj->y, obj->width, obj->height};
	rect_t new_rect = {obj->x, obj->y, new_w, new_h};
	rect_t rect = rect_union(old_rect, new_rect);

	compositor_add_damage(obj->layer, rect.x, rect.y, rect.w, rect.h);

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
