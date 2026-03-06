#include "element.h"
#include <gui/core/object/object.h>

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <gui/ui/text/text.h>

void element_draw(element_t *el)
{
	if (!el->buffer)
		return;

	uint32_t bg;
	switch (el->state)
	{
	case ELEMENT_NORMAL:
		bg = rgb(150, 150, 150);
		break;

	case ELEMENT_HOVER:
		bg = rgb(160, 160, 160);
		break;
	case ELEMENT_PRESSED:
		bg = rgb(140, 140, 140);
		break;
	default:
		bg = el->bg_color;
		break;
	}
	for (int i = 0; i < el->width * el->height; i++)
		el->buffer[i] = bg;

	if (!el->text)
		return;

	text_t *txt = element_create_text(0, 0, el->text);
	if (!txt)
		return;

	int text_x = (el->width - txt->width) / 2;
	int text_y = (el->height - txt->height) / 2;

	for (int row = 0; row < txt->height; row++)
		for (int col = 0; col < txt->width; col++)
		{
			int dx = text_x + col;
			int dy = text_y + row;
			if (dx < 0 || dy < 0 || dx >= el->width || dy >= el->height)
				continue;
			uint32_t px = txt->buffer[row * txt->width + col];
			if (px == rgb(255, 255, 255))
				el->buffer[dy * el->width + dx] = el->text_color;
		}

	free(txt->buffer);
	free(txt->text);
	free(txt);
}

static void element_on_enter(element_t *el)
{
	el->state = ELEMENT_HOVER;
	el->needs_redraw = true;
	element_mark_dirty(el);
	if (el->owner)
		object_flush(el->owner);
}

static void element_on_leave(element_t *el)
{
	if (el->state == ELEMENT_PRESSED)
		return;
	el->state = ELEMENT_NORMAL;
	el->needs_redraw = true;
	element_mark_dirty(el);
	if (el->owner)
		object_flush(el->owner);
}

static void element_on_down(element_t *el)
{
	el->state = ELEMENT_PRESSED;
	el->needs_redraw = true;
	element_mark_dirty(el);
	if (el->owner)
		object_flush(el->owner);
}

static void element_on_up(element_t *el)
{

	if (el->state == ELEMENT_PRESSED && el->on_click)
		el->on_click();
	el->state = ELEMENT_HOVER;
	el->needs_redraw = true;
	element_mark_dirty(el);
	if (el->owner)
		object_flush(el->owner);
}

void element_init(element_t *el)
{
	el->draw = element_draw;
	el->on_mouse_enter = NULL;
	el->on_mouse_leave = NULL;
	el->on_mouse_down = NULL;
	el->on_mouse_up = NULL;
	el->owner = NULL;
	el->dirty_rect = (dirty_rect_t){0};

	el->on_mouse_enter = element_on_enter;
	el->on_mouse_leave = element_on_leave;
	el->on_mouse_down = element_on_down;
	el->on_mouse_up = element_on_up;

	el->state = ELEMENT_NORMAL;

	el->bg_color = rgb(180, 180, 180);
	el->text_color = rgb(30, 30, 30);
	el->on_click = NULL;
}

element_t *element_create(int x, int y, int w, int h)
{
	element_t *el = malloc(sizeof(element_t));
	if (!el)
		return NULL;

	memset(el, 0, sizeof(element_t));

	el->x = x;
	el->y = y;
	el->width = w;
	el->height = h;

	el->state = ELEMENT_NORMAL;

	el->bg_color = rgb(180, 180, 180);
	el->text_color = rgb(30, 30, 30);

	el->draw = element_draw;

	el->on_mouse_enter = element_on_enter;
	el->on_mouse_leave = element_on_leave;
	el->on_mouse_down = element_on_down;
	el->on_mouse_up = element_on_up;

	el->buffer = malloc(w * h * sizeof(uint32_t));

	if (!el->buffer)
	{
		free(el);
		return NULL;
	}

	return el;
}

void element_destroy(element_t *el)
{
	if (!el)
		return;

	if (el->buffer)
		free(el->buffer);

	if (el->text)
		free(el->text);

	free(el);
}
