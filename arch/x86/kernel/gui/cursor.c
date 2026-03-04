#include "cursor.h"
#include "printk.h"
#include <stdlib.h>
#include <string.h>

cursor_t *cursor_create(int w, int h, uint32_t color_outer, uint32_t color_inner)
{
	cursor_t *c = malloc(sizeof(cursor_t));
	if (!c)
	{
		printk("failed to create");
		return NULL;
	}

	c->width = w;
	c->height = h;
	c->x = 0;
	c->y = 0;

	// transparent cursor object
	// __asm__ volatile("cli");
	c->surface = object_create(0, 0, w, h, 0);
	// __asm__ volatile("sti");
	if (!c->surface)
	{
		printk("no cursor surface");
		free(c);
		return NULL;
	}

	/// clearing buffer
	memset(c->surface->buffer, 0, w * h * sizeof(uint32_t));

	// outer frame
	for (int i = 0; i < w * h; i++)
		c->surface->buffer[i] = color_outer;

	// inner square
	int iw = w / 2, ih = h / 2;
	for (int y = 0; y < ih; y++)
		for (int x = 0; x < iw; x++)
			c->surface->buffer[(y + h / 4) * w + (x + w / 4)] = color_inner;

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
	{
		return;
	}
	c->x = x;
	c->y = y;
	// printk("new x: %d new y: %d", x, y);
	compositor_move_object(c->surface, x, y);
}
