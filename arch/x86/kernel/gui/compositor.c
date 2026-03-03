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
}

void compositor_remove(object_t *obj)
{
	for (int i = 0; i < compositor.count; i++)
	{
		if (compositor.objects[i] == obj)
		{
			compositor.objects[i] = compositor.objects[compositor.count - 1];
			compositor.count--;
			return;
		}
	}
}

void compositor_render()
{
	// clearing backbuffer
	memset(framebuffer_back, 0,
	       fb_width * fb_height * sizeof(uint32_t));

	for (int i = 0; i < compositor.count; i++)
	{
		object_t *obj = compositor.objects[i];

		for (int row = 0; row < obj->height; row++)
		{
			for (int col = 0; col < obj->width; col++)
			{

				int screen_x = obj->x + col;
				int screen_y = obj->y + row;

				if (screen_x < 0 || screen_y < 0 ||
				    screen_x >= fb_width ||
				    screen_y >= fb_height)
					continue;

				uint32_t src =
				    obj->buffer[row * obj->width + col];

				uint32_t *dst =
				    framebuffer_back +
				    screen_y * fb_width + screen_x;

				*dst = color_blend(src, *dst);
			}
		}
	}
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
