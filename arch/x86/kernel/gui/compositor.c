#include "compositor.h"
#include "screen.h"
#include <string.h>

static compositor_t compositor;

void compositor_init()
{
	compositor.count = 0;
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

void compositor_render()
{
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

static void compositor_add_damage(int x, int y, int w, int h)
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

void compositor_move_object(object_t *obj, int new_x, int new_y)
{
	// damage to old area
	compositor_add_damage(obj->x, obj->y, obj->width, obj->height);

	obj->x = new_x;
	obj->y = new_y;

	// damage to new area
	compositor_add_damage(obj->x, obj->y, obj->width, obj->height);
}
