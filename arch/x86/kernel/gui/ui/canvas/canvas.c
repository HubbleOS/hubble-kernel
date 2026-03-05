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

void canvas_draw_line(canvas_t *c, int x1, int y1, int x2, int y2, uint32_t color)
{
	int dx = x2 - x1;
	int dy = y2 - y1;
	int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
	float xinc = dx / (float)steps;
	float yinc = dy / (float)steps;
	float x = x1;
	float y = y1;
	for (int i = 0; i <= steps; i++)
	{
		canvas_set_pixel(c, x, y, color);
		x += xinc;
		y += yinc;
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
