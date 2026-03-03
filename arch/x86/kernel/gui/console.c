#include "console.h"
#include <utils/color.h>
#include <string.h>
#include <stdlib.h>

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 16

console_t *console_create(int x, int y, int w, int h, color_t font, color_t bg)
{
	console_t *con = malloc(sizeof(console_t));
	if (!con)
		return NULL;

	con->win = window_create(x, y, w, h);
	if (!con->win)
	{
		free(con);
		return NULL;
	}

	con->cursor_x = 0;
	con->cursor_y = 0;
	con->font_color = font;
	con->bg_color = bg;

	object_t *obj = con->win->surface;

	window_drawRect(con->win, 0, 0, obj->width, obj->height, con->bg_color);

	return con;
}

void console_scroll(console_t *con)
{
	object_t *obj = con->win->surface;

	int w = obj->width;
	int h = obj->height;
	uint32_t *buf = obj->buffer;

	memmove(buf, buf + w * CHAR_HEIGHT, (h - CHAR_HEIGHT) * w * sizeof(uint32_t));

	// очистка нижнего ряда
	for (int y = h - CHAR_HEIGHT; y < h; y++)
		for (int x = 0; x < w; x++)
			buf[y * w + x] = con->bg_color;

	con->cursor_y = h - CHAR_HEIGHT; // ставим курсор на нижнюю строку
}

#include <utils/font.h>

void console_draw_char(console_t *con, char c)
{
	int x = con->cursor_x;
	int y = con->cursor_y;
	int w = CHAR_WIDTH;
	int h = CHAR_HEIGHT;
	color_t color = con->font_color;

	object_t *obj = con->win->surface;

	framebuffer_info_t fb;
	fb.base = (void *)obj->buffer;
	fb.width = obj->width;
	fb.height = obj->height;
	fb.pitch = obj->width * sizeof(uint32_t);
	fb.bpp = 32;

	draw_char(&fb, c, x, y, w, h, color);
	return;
}

void console_putc(console_t *con, char c)
{
	if (!con || !con->win)
		return;

	console_draw_char(con, c);
	con->cursor_x += CHAR_WIDTH;

	object_t *obj = con->win->surface;

	if (con->cursor_x + CHAR_WIDTH > obj->width)
	{
		con->cursor_x = 0;
		con->cursor_y += CHAR_HEIGHT;
	}

	if (con->cursor_y + CHAR_HEIGHT > obj->height)
		console_scroll(con);
}

void console_write(console_t *con, const char *str, size_t len)
{
	for (size_t i = 0; i < len; i++)
		console_putc(con, str[i]);
}

void console_clear(console_t *con)
{
	if (!con || !con->win)
		return;

	object_t *obj = con->win->surface;

	window_drawRect(con->win, 0, 0, obj->width, obj->height, con->bg_color);
	con->cursor_x = 0;
	con->cursor_y = 0;
}
