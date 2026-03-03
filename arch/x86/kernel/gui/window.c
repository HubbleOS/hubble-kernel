#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "window.h"
#include "screen.h"

#include "object.h"
#include "compositor.h"

#define MAX_WINDOWS 256
static window_t windows[MAX_WINDOWS];
static int window_count = 0;

window_t *window_create(int x, int y, int w, int h)
{
	window_t *win = malloc(sizeof(window_t));
	if (!win)
		return NULL;

	win->surface = object_create(x, y, w, h);
	if (!win->surface)
	{
		free(win);
		return NULL;
	}

	win->visible = true;
	win->focused = false;
	strcpy(win->title, "Window");

	compositor_add(win->surface);

	return win;
}

void window_destroy(window_t *win)
{
	if (!win)
		return;

	compositor_remove(win->surface);
	object_destroy(win->surface);
	free(win);
}

int window_drawPixel(window_t *win, int x, int y, color_t color)
{
	object_t *obj = win->surface;

	if (x < 0 || y < 0 ||
	    x >= obj->width || y >= obj->height)
		return -1;

	obj->buffer[y * obj->width + x] = color;
	return 0;
}

int window_drawRect(window_t *win, int x, int y, int w, int h, color_t color)
{
	object_t *obj = win->surface;

	if (w <= 0 || h <= 0)
		return -1;

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

	if (x + w > obj->width)
		w = obj->width - x;
	if (y + h > obj->height)
		h = obj->height - y;
	if (w <= 0 || h <= 0)
		return -1;

	for (int row = 0; row < h; row++)
	{
		uint32_t *pixel = obj->buffer + (y + row) * obj->width + x;
		for (int col = 0; col < w; col++)
			pixel[col] = color;
	}

	return 0;
}

void window_move(window_t *win, int x, int y)
{
	compositor_move_object(win->surface, x, y);
}

void window_focus(window_t *win)
{
	win->focused = true;
	compositor_bring_to_front(win->surface);
}
