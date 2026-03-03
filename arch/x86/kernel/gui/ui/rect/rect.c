#include "rect.h"

#include <stdint.h>
#include <stdlib.h>

element_t *create_rect(int x, int y, int w, int h, color_t color)
{
	element_t *rect = malloc(sizeof(element_t));
	if (!rect)
		return NULL;

	rect->x = x;
	rect->y = y;
	rect->width = w;
	rect->height = h;

	rect->buffer = malloc(w * h * sizeof(uint32_t));
	if (!rect->buffer)
	{
		free(rect);
		return NULL;
	}

	for (int i = 0; i < w * h; i++)
		rect->buffer[i] = color; // ARGB

	return rect;
}

void destroy_rect(element_t *rect)
{
	free(rect->buffer);
	free(rect);
}
