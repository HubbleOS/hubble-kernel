#include "element.h"
#include <gui/core/object/object.h>

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <gui/ui/text/fonts/font.h>

static void element_apply_style(element_t *el, element_style_t *override)
{
	if (!el->style_set || !el->style_set->normal)
		return;

	element_style_t *base = el->style_set->normal;

	el->style->background_color = base->background_color;
	el->style->text_color = base->text_color;

	if (override && override->background_color)
		el->style->background_color = override->background_color;
	if (override && override->text_color)
		el->style->text_color = override->text_color;
}

void element_draw(element_t *el)
{
	if (!el->buffer || !el->style)
		return;

	uint32_t bg = el->style->background_color
			  ? el->style->background_color
			  : el->bg_color;

	for (int i = 0; i < el->width * el->height; i++)
		el->buffer[i] = bg;

	if (!el->text)
		return;

	size_t len = strlen(el->text);
	int gx = (el->width - (int)(len * 8)) / 2;
	int gy = (el->height - 8) / 2;

	for (size_t i = 0; i < len; i++)
	{
		uint8_t *glyph = (uint8_t *)get_glyph(el->text[i]);
		if (!glyph)
			glyph = (uint8_t *)get_glyph('?');

		for (int row = 0; row < 8; row++)
		{
			uint8_t line = glyph[row];
			for (int col = 0; col < 8; col++)
			{
				if (!(line & (0x80 >> col)))
					continue;
				int px = gx + (int)i * 8 + col;
				int py = gy + row;
				if (px < 0 || py < 0 || px >= el->width || py >= el->height)
					continue;
				el->buffer[py * el->width + px] = el->text_color;
			}
		}
	}
}

static void element_on_enter(element_t *el)
{
	el->state = ELEMENT_HOVER;
	if (el->style_set)
		element_apply_style(el, el->style_set->hover);
	el->needs_redraw = true;
	element_mark_dirty(el);
	if (el->owner)
		object_flush(el->owner);
}

static void element_on_leave(element_t *el)
{
	el->state = ELEMENT_NORMAL;
	if (el->style_set)
		element_apply_style(el, NULL);
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

	el->style = malloc(sizeof(element_style_t));
	if (!el->style)
	{
		free(el);
		return NULL;
	}

	element_style_init(el->style);

	el->style_set = malloc(sizeof(element_style_set_t));
	if (!el->style_set)
	{
		free(el->style);
		free(el);
		return NULL;
	}

	el->style_set->normal = malloc(sizeof(element_style_t));
	el->style_set->hover = malloc(sizeof(element_style_t));
	el->style_set->pressed = malloc(sizeof(element_style_t));

	element_style_init(el->style_set->normal);
	element_style_init(el->style_set->hover);
	element_style_init(el->style_set->pressed);

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

	if (el->style)
		free(el->style);

	if (el->style_set)
	{
		if (el->style_set->normal)
			free(el->style_set->normal);
		if (el->style_set->hover)
			free(el->style_set->hover);
		if (el->style_set->pressed)
			free(el->style_set->pressed);
		free(el->style_set);
	}

	if (el->buffer)
		free(el->buffer);

	if (el->text)
		free(el->text);

	free(el);
}
