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
	{
		compositor.layers[i].count = 0;
		compositor.layers[i].buffer = malloc(fb_width * fb_height * sizeof(uint32_t));
		if (!compositor.layers[i].buffer)
		{

			printk("Error: Failed to allocate compositor buffer\n");
			return;
		}

		memset(compositor.layers[i].buffer, 0, fb_width * fb_height * sizeof(uint32_t));
	}
	compositor.dirty_count = 0;
}

typedef struct
{
	rect_t dirty[MAX_DIRTY];
	int dirty_count;
} layer_dirty_t;

layer_dirty_t layer_dirty[MAX_LAYERS] = {0};

void compositor_add_damage(int layer, int x, int y, int w, int h)
{
	if (layer < 0 || layer >= MAX_LAYERS)
		return;

	layer_dirty_t *ld = &layer_dirty[layer];
	if (ld->dirty_count >= MAX_DIRTY || w <= 0 || h <= 0)
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

	ld->dirty[ld->dirty_count++] = (rect_t){x, y, w, h};
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
			compositor_add_damage(obj->layer, obj->x, obj->y, obj->width, obj->height);
			l->objects[i] = l->objects[l->count - 1];
			l->count--;
			return;
		}
	}
}

static inline bool rects_overlap(int ax1, int ay1, int ax2, int ay2,
				 int bx1, int by1, int bx2, int by2)
{
	return !(ax2 <= bx1 || ax1 >= bx2 || ay2 <= by1 || ay1 >= by2);
}

void compositor_render()
{
	// 1. Собираем глобальный dirty из всех слоёв
	rect_t global_dirty[MAX_DIRTY];
	int global_count = 0;

	for (int l = 0; l < MAX_LAYERS; l++)
	{
		layer_dirty_t *ld = &layer_dirty[l];
		if (ld->dirty_count == 0)
			continue;

		merge_dirty_rects(ld->dirty, &ld->dirty_count);

		// 2. Рендерим объекты слоя в его собственный буфер
		for (int d = 0; d < ld->dirty_count; d++)
		{
			rect_t r = ld->dirty[d];
			uint32_t *layer_buf = compositor.layers[l].buffer;

			// Очищаем регион в буфере слоя
			for (int y = r.y; y < r.y + r.h; y++)
				memset(layer_buf + y * fb_width + r.x, 0, r.w * sizeof(uint32_t));

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
					uint32_t *obj_row = obj->buffer + (y - obj->y) * obj->width + (start_x - obj->x);
					uint32_t *dst_row = layer_buf + y * fb_width + start_x;
					int w = end_x - start_x;

					memcpy(dst_row, obj_row, w * sizeof(uint32_t));
				}
			}

			// Запоминаем регион для финальной компоновки
			if (global_count < MAX_DIRTY)
				global_dirty[global_count++] = r;
		}

		ld->dirty_count = 0;
	}

	if (global_count == 0)
		return;

	merge_dirty_rects(global_dirty, &global_count);

	// 3. Компонуем все слои снизу вверх в framebuffer_back только в dirty-регионах
	for (int d = 0; d < global_count; d++)
	{
		rect_t r = global_dirty[d];

		for (int y = r.y; y < r.y + r.h; y++)
		{
			uint32_t *dst = framebuffer_back + y * fb_width + r.x;

			// Начинаем с нуля
			memset(dst, 0, r.w * sizeof(uint32_t));

			for (int l = 0; l < MAX_LAYERS; l++)
			{
				uint32_t *src = compositor.layers[l].buffer + y * fb_width + r.x;
				for (int x = 0; x < r.w; x++)
					dst[x] = color_blend(src[x], dst[x]);
			}
		}

		screen_present_rect(r.x, r.y, r.w, r.h);
	}
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
	rect_t old_rect = {obj->x, obj->y, obj->width, obj->height};
	rect_t new_rect = {new_x, new_y, obj->width, obj->height};
	rect_t rect = rect_union(old_rect, new_rect);

	compositor_add_damage(obj->layer, rect.x, rect.y, rect.w, rect.h);

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
