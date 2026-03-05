#include "cursor.h"
#include <stdlib.h>
#include <string.h>

#include <gui/ui/rect/rect.h>

cursor_t *cursor_create(int w, int h, uint32_t color_outer, uint32_t color_inner)
{
	cursor_t *c = malloc(sizeof(cursor_t));
	if (!c)
		return NULL;

	// transparent cursor object
	c->surface = object_create(0, 0, w, h, color_outer);

	if (!c->surface)
	{
		free(c);
		return NULL;
	}

	int iw = w / 2, ih = h / 2;

	element_t *rect = create_rect(iw - 2, ih - 2, 4, 4, color_inner);
	object_add_element(c->surface, rect);
	free(rect);

	compositor_add(c->surface, LAYER_CURSOR);
	return c;
}

void cursor_destroy(cursor_t *c)
{
	if (!c)
		return;

	for (int i = 0; i < c->surface->element_count; i++)
		free(c->surface->elements[i]->buffer);
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
