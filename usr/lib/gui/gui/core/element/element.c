#include "element.h"

#include <gui/core/object/object.h>

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include <gui/ui/text/fonts/font.h>

#include "styles/border_radius.h"

static bool point_in_rounded_rect(int x, int y, int w, int h, int r)
{
	int cx, cy;

	if (x < r && y < r)
	{
		cx = r;
		cy = r;
	}
	else if (x >= w - r && y < r)
	{
		cx = w - r - 1;
		cy = r;
	}
	else if (x < r && y >= h - r)
	{
		cx = r;
		cy = h - r - 1;
	}
	else if (x >= w - r && y >= h - r)
	{
		cx = w - r - 1;
		cy = h - r - 1;
	}
	else
		return false;

	int dx = x - cx;
	int dy = y - cy;
	return dx * dx + dy * dy > r * r;
}

void element_draw(element_t *el)
{
	if (!el->buffer)
		return;

	uint32_t bg = el->active_style.background_color
			  ? el->active_style.background_color
			  : el->bg_color;

	int r = el->active_style.border_radius;

	for (int y = 0; y < el->height; y++)
	{
		for (int x = 0; x < el->width; x++)
		{
			if (r > 0 && point_in_rounded_rect(x, y, el->width, el->height, r))
				el->buffer[y * el->width + x] = 0; // transparent
			else
				el->buffer[y * el->width + x] = bg;
		}
	}

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

void element_redraw(element_t *el)
{
	el->needs_redraw = true;
	element_mark_dirty(el);
	if (el->owner)
		object_flush(el->owner);
}
static void element_on_enter(element_t *el)
{
	el->state = ELEMENT_HOVER;
	if (el->style_set)
		element_apply_style(el, el->style_set->hover);

	element_redraw(el);
}

static void element_on_leave(element_t *el)
{
	el->state = ELEMENT_NORMAL;
	if (el->style_set)
		element_apply_style(el, NULL);

	element_redraw(el);
}

#include <gui/dev/keyboard/keyboard.h>

static void element_on_down(element_t *el)
{
	el->state = ELEMENT_PRESSED;
	if (el->style_set)
		element_apply_style(el, el->style_set->pressed);

#ifdef GUI_DEMO

	if (el->type == UI_TEXTBOX)
		g_keyboard.focused_el = el;

#endif

	element_redraw(el);
}

static void element_on_up(element_t *el)
{
	if (el->state == ELEMENT_PRESSED && el->on_click)
		el->on_click();
	el->state = ELEMENT_HOVER;
	if (el->style_set)
		element_apply_style(el, el->style_set->hover);
	element_redraw(el);
}

static void element_key_down(element_t *el)
{

	element_redraw(el);
}

element_t *element_create(int x, int y, int w, int h)
{
	element_t *el = malloc(sizeof(element_t));
	if (!el)
		return NULL;

	memset(el, 0, sizeof(element_t));

	el->style_set = malloc(sizeof(element_style_set_t));
	if (!el->style_set)
	{
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

	el->on_key_down = NULL;
	el->on_key_up = NULL;

	el->on_key_special = NULL;
	el->on_key_char = NULL;

	el->buffer = malloc(w * h * sizeof(uint32_t));
	if (!el->buffer)
	{
		free(el->style_set->normal);
		free(el->style_set->hover);
		free(el->style_set->pressed);
		free(el->style_set);
		free(el);
		return NULL;
	}

	element_apply_style(el, NULL);

	return el;
}

void element_destroy(element_t *el)
{
	if (!el)
		return;

	if (el->style_set)
	{
		free(el->style_set->normal);
		free(el->style_set->hover);
		free(el->style_set->pressed);
		free(el->style_set);
	}

	free(el->buffer);
	free(el->text);
	free(el);
}

void element_set_text(element_t *el, const char *text)
{
	el->text = strdup(text);
	el->needs_redraw = true;
}
