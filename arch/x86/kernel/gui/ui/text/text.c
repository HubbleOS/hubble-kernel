#include "text.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "fonts/font.h"
#include <gui/utils/color/color.h>

#define FONT_SIZE 8

static void draw_pixel_array(uint8_t *glyph, element_t *el, int x, int y, color_t font_color)
{
	for (int row = 0; row < FONT_SIZE; ++row)
	{
		uint8_t line = glyph[row];
		for (int col = 0; col < FONT_SIZE; ++col)
		{
			if (line & (0x80 >> col))
			{
				int px = x + col;
				int py = y + row;
				if (px < el->width && py < el->height)
					((uint32_t *)el->buffer)[py * el->width + px] = font_color;
			}
		}
	}
}

static void draw_text_in_element(element_t *el)
{
	if (!el || !el->text || !el->buffer)
		return;

	color_t bg = rgb(50, 50, 50);
	color_t fg = rgb(255, 255, 255);

	for (int i = 0; i < el->width * el->height; i++)
		el->buffer[i] = bg;

	for (int i = 0; el->text[i]; ++i)
	{
		uint8_t *glyph = (uint8_t *)get_glyph(el->text[i]);
		if (!glyph)
			glyph = (uint8_t *)get_glyph('!');
		draw_pixel_array(glyph, el, i * FONT_SIZE, 0, fg);
	}
}

text_t *element_create_text(int x, int y, const char *text)
{
	if (!text)
		return NULL;

	size_t len = strlen(text);

	text_t *el = element_create(x, y, len * FONT_SIZE, FONT_SIZE);
	if (!el)
		return NULL;

	el->type = UI_LABEL;
	el->draw = draw_text_in_element;

	el->text = strdup(text);
	if (!el->text)
	{
		element_destroy((element_t *)el);
		return NULL;
	}

	el->dirty_rect = (dirty_rect_t){0, 0, el->width, el->height, true};

	// temp
	draw_text_in_element(el);

	return el;
}

void text_destroy(element_t *el)
{
	element_destroy(el);
}
