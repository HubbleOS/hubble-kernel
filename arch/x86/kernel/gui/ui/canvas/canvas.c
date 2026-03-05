#include "canvas.h"
#include <stdlib.h>
#include <string.h>
#include "../text/text.h"
#include <gui/utils/color/color.h>

static void canvas_draw_element(element_t *el)
{
	(void)el;
}

canvas_t *canvas_create(int x, int y, int width, int height)
{
	canvas_t *c = malloc(sizeof(canvas_t));
	if (!c)
		return NULL;

	c->base.x = x;
	c->base.y = y;
	c->base.width = width;
	c->base.height = height;
	c->base.type = UI_RECT;
	c->base.draw = canvas_draw_element;
	c->base.event = NULL;

	c->base.buffer = malloc(width * height * sizeof(uint32_t));
	if (!c->base.buffer)
	{
		free(c);
		return NULL;
	}

	memset(c->base.buffer, 0, width * height * sizeof(uint32_t));
	return c;
}

void canvas_clear(canvas_t *c, uint32_t color)
{
	if (!c || !c->base.buffer)
		return;

	for (int i = 0; i < c->base.width * c->base.height; i++)
		c->base.buffer[i] = color;
}

void canvas_set_pixel(canvas_t *c, int x, int y, uint32_t color)
{
	if (!c || !c->base.buffer)
		return;

	if (x < 0 || y < 0 || x >= c->base.width || y >= c->base.height)
		return;

	c->base.buffer[y * c->base.width + x] = color;
}

void canvas_draw_line(canvas_t *c, int x0, int y0, int x1, int y1, uint32_t color)
{
	int dx = abs(x1 - x0);
	int dy = abs(y1 - y0);

	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;

	int err = dx - dy;

	while (1)
	{
		canvas_set_pixel(c, x0, y0, color);

		if (x0 == x1 && y0 == y1)
			break;

		int e2 = err * 2;

		if (e2 > -dy)
		{
			err -= dy;
			x0 += sx;
		}

		if (e2 < dx)
		{
			err += dx;
			y0 += sy;
		}
	}
}
void canvas_draw_rect(canvas_t *c, int x, int y, int w, int h, uint32_t color)
{
	for (int row = 0; row < h; row++)
		for (int col = 0; col < w; col++)
			canvas_set_pixel(c, x + col, y + row, color);
}

void canvas_draw_text(canvas_t *c, int x, int y, const char *text, uint32_t color)
{
	if (!c || !text)
		return;

	text_t *txt = element_create_text(0, 0, text);
	if (!txt)
		return;

	for (int row = 0; row < txt->base.height; row++)
	{
		for (int col = 0; col < txt->base.width; col++)
		{
			uint32_t px = txt->base.buffer[row * txt->base.width + col];
			if (px == rgb(255, 255, 255))
				canvas_set_pixel(c, x + col, y + row, color);
		}
	}

	free(txt->base.buffer);
	free(txt->base.text);
	free(txt);
}
