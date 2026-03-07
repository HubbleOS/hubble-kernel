#include "cursor.h"
// #include "printk.h"
#include <stdlib.h>
#include <string.h>

cursor_t *cursor_create(int w, int h, uint32_t color_outer, uint32_t color_inner)
{
	cursor_t *c = malloc(sizeof(cursor_t));
	if (!c)
	{
		// printk("failed to create");
		return NULL;
	}

	c->surface = object_create(0, 0, w, h, color_outer);
	if (!c->surface)
	{
		// printk("no cursor surface");
		free(c);
		return NULL;
	}

	int iw = w / 2, ih = h / 2;

	element_t *dot = malloc(sizeof(element_t));
	element_init(dot);

	dot->x = iw - 2;
	dot->y = ih - 2;
	dot->width = 4;
	dot->height = 4;
	dot->type = UI_RECT;
	dot->bg_color = color_inner;

	dot->on_mouse_enter = NULL;
	dot->on_mouse_leave = NULL;
	dot->on_mouse_down = NULL;
	dot->on_mouse_up = NULL;

	dot->buffer = malloc(4 * 4 * sizeof(uint32_t));
	for (int i = 0; i < 4 * 4; i++)
		dot->buffer[i] = color_inner;

	object_add_element(c->surface, dot);
	compositor_add(c->surface, LAYER_CURSOR);
	return c;
}

void cursor_destroy(cursor_t *c)
{
	if (!c)
		return;

	for (int i = 0; i < c->surface->element_count; i++)
	{
		free(c->surface->elements[i]->buffer);
		free(c->surface->elements[i]);
	}
	free(c->surface->elements);

	compositor_remove(c->surface, LAYER_CURSOR);
	object_destroy(c->surface);
	free(c);
}

void cursor_move(cursor_t *c, int x, int y)
{
	if (!c)
		return;
	compositor_move_object(c->surface, x, y);
}
