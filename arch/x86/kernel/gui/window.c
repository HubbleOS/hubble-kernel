#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "window.h"
#include "screen.h"

#include "object.h"
#include "compositor.h"

window_t *window_create(int x, int y, int w, int h)
{
	window_t *win = malloc(sizeof(window_t));
	if (!win)
		return NULL;

	win->surface = object_create(x, y, w, h, rgb(255, 255, 255));

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

int window_addElement(window_t *win, element_t *el)
{
	if (!win || !el)
		return -1;

	object_add_element(win->surface, el);
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

void window_resize(window_t *win, int w, int h)
{
	compositor_change_size_object(win->surface, w, h);
	object_redraw_elements(win->surface);
}
