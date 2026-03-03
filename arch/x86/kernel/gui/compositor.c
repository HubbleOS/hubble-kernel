#include "compositor.h"
#include "screen.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

static compositor_t compositor;

static bool rects_intersect(rect_t a, rect_t b)
{
	return !(a.x + a.w <= b.x || b.x + b.w <= a.x ||
		 a.y + a.h <= b.y || b.y + b.h <= a.y);
}

static rect_t rect_union(rect_t a, rect_t b)
{
	int x1 = a.x < b.x ? a.x : b.x;
	int y1 = a.y < b.y ? a.y : b.y;
	int x2 = (a.x + a.w) > (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
	int y2 = (a.y + a.h) > (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);

	rect_t r = {x1, y1, x2 - x1, y2 - y1};
	return r;
}

void compositor_add_damage(int x, int y, int w, int h)
{
	if (compositor.dirty_count >= MAX_DIRTY)
		return;

	if (w <= 0 || h <= 0)
		return;

	// clamp to screen
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
	compositor.count = 0;
	compositor.dirty_count = 0;
}

void compositor_add(object_t *obj)
{
	if (compositor.count >= MAX_OBJECTS)
		return;

	compositor.objects[compositor.count++] = obj;

	compositor_add_damage(obj->x, obj->y, obj->width, obj->height);
}

void compositor_remove(object_t *obj)
{
	for (int i = 0; i < compositor.count; i++)
	{
		if (compositor.objects[i] == obj)
		{
			// damage to old area
			compositor_add_damage(obj->x, obj->y, obj->width, obj->height);

			compositor.objects[i] = compositor.objects[compositor.count - 1];
			compositor.count--;
			return;
		}
	}
}

void merge_dirty_rects(rect_t *dirty, int *count)
{
	bool merged_any;
	do
	{
		merged_any = false;
		for (int i = 0; i < *count; i++)
		{
			for (int j = i + 1; j < *count; j++)
			{
				if (rects_intersect(dirty[i], dirty[j]))
				{
					dirty[i] = rect_union(dirty[i], dirty[j]);
					// shift the array, remove j
					for (int k = j; k < *count - 1; k++)
						dirty[k] = dirty[k + 1];
					(*count)--;
					merged_any = true;
					break;
				}
			}
			if (merged_any)
				break;
		}
	} while (merged_any);
}

void compositor_render()
{
	merge_dirty_rects(compositor.dirty, &compositor.dirty_count);

	for (int d = 0; d < compositor.dirty_count; d++)
	{
		rect_t r = compositor.dirty[d];

		// 1. clear only the dirty region
		for (int row = r.y; row < r.y + r.h; row++)
		{
			uint32_t *dst = framebuffer_back + row * fb_width + r.x;
			memset(dst, 0, r.w * sizeof(uint32_t));
		}

		// 2. redraw objects that intersect
		for (int i = 0; i < compositor.count; i++)
		{
			object_t *obj = compositor.objects[i];

			int obj_x1 = obj->x;
			int obj_y1 = obj->y;
			int obj_x2 = obj->x + obj->width;
			int obj_y2 = obj->y + obj->height;

			int r_x2 = r.x + r.w;
			int r_y2 = r.y + r.h;

			// intersection check
			if (obj_x1 >= r_x2 || obj_x2 <= r.x ||
			    obj_y1 >= r_y2 || obj_y2 <= r.y)
				continue;

			// calculate the intersection area
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

					uint32_t src =
					    obj->buffer[obj_py * obj->width + obj_px];

					uint32_t *dst =
					    framebuffer_back + y * fb_width + x;

					*dst = color_blend(src, *dst);
				}
			}
		}

		// 3. present only this dirty rect to the actual framebuffer
		screen_present_rect(r.x, r.y, r.w, r.h);
	}

	compositor.dirty_count = 0;
}

void compositor_bring_to_front(object_t *obj)
{
	int index = -1;

	for (int i = 0; i < compositor.count; i++)
	{
		if (compositor.objects[i] == obj)
		{
			index = i;
			break;
		}
	}

	if (index == -1)
		return;

	// already on top
	if (index == compositor.count - 1)
		return;

	// move everything to the left
	for (int i = index; i < compositor.count - 1; i++)
		compositor.objects[i] = compositor.objects[i + 1];

	// put it at the end
	compositor.objects[compositor.count - 1] = obj;
}

void compositor_move_object(object_t *obj, int new_x, int new_y)
{
	// damage to old area
	compositor_add_damage(obj->x, obj->y, obj->width, obj->height);

	obj->x = new_x;
	obj->y = new_y;

	// damage to new area
	compositor_add_damage(obj->x, obj->y, obj->width, obj->height);
}

void compositor_change_size_object(object_t *obj, int new_w, int new_h)
{
	// damage to old area
	compositor_add_damage(obj->x, obj->y, obj->width, obj->height);

	// allocate a new buffer
	uint32_t *new_buf = malloc(new_w * new_h * sizeof(uint32_t));
	if (!new_buf)
		return; // do not change if there is not enough memory

	memset(new_buf, 0, new_w * new_h * sizeof(uint32_t));

	for (int i = 0; i < new_w * new_h; i++)
		new_buf[i] = obj->bg_color;

	// copy old content
	int copy_w = obj->width < new_w ? obj->width : new_w;
	int copy_h = obj->height < new_h ? obj->height : new_h;
	for (int y = 0; y < copy_h; y++)
		memcpy(new_buf + y * new_w, obj->buffer + y * obj->width, copy_w * sizeof(uint32_t));

	// replaceable buffer
	free(obj->buffer);
	obj->buffer = new_buf;

	obj->width = new_w;
	obj->height = new_h;

	object_redraw_elements(obj);

	// damage to new area
	compositor_add_damage(obj->x, obj->y, obj->width, obj->height);
}
