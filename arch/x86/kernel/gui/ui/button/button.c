#include "button.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../text/text.h"

void button_draw(element_t *el)
{
	button_t *btn = (button_t *)el;

	uint32_t bg;

	switch (btn->state)
	{
	case BUTTON_NORMAL:
		bg = rgb(180, 180, 180);
		break;
	case BUTTON_HOVER:
		bg = rgb(210, 210, 210);
		break;
	case BUTTON_PRESSED:
		bg = rgb(140, 140, 140);
		break;
	}

	// Background fill
	for (int y = 0; y < el->height; y++)
		for (int x = 0; x < el->width; x++)
			el->buffer[y * el->width + x] = bg;

	// Drawing text
	if (!el->text)
		return;

	text_t *txt = element_create_text(0, 0, el->text);
	if (!txt)
		return;

	// Centering the text on a button
	int text_x = (el->width - txt->base.width) / 2;
	int text_y = (el->height - txt->base.height) / 2;

	for (int row = 0; row < txt->base.height; row++)
	{
		for (int col = 0; col < txt->base.width; col++)
		{
			int dx = text_x + col;
			int dy = text_y + row;

			if (dx < 0 || dy < 0 || dx >= el->width || dy >= el->height)
				continue;

			uint32_t px = txt->base.buffer[row * txt->base.width + col];
			if (px == rgb(255, 255, 255))
				el->buffer[dy * el->width + dx] = rgb(30, 30, 30); // цвет текста
		}
	}

	free(txt->base.buffer);
	free(txt->base.text);
	free(txt);
}

void button_event(element_t *el, int event)
{
	button_t *btn = (button_t *)el;

	switch (event)
	{
	case UI_EVENT_MOUSE_ENTER:
		btn->state = BUTTON_HOVER;
		break;

	case UI_EVENT_MOUSE_LEAVE:
		btn->state = BUTTON_NORMAL;
		break;

	case UI_EVENT_MOUSE_DOWN:
		btn->state = BUTTON_PRESSED;
		break;

	case UI_EVENT_MOUSE_UP:
		if (btn->state == BUTTON_PRESSED && btn->on_click)
			btn->on_click(btn->user_data);

		btn->state = BUTTON_HOVER;
		break;
	}
}

button_t *button_create(int x, int y, int w, int h, const char *text)
{
	button_t *btn = malloc(sizeof(button_t));
	if (!btn)
		return NULL;

	element_t *el = &btn->base;

	el->x = x;
	el->y = y;
	el->width = w;
	el->height = h;
	el->type = UI_BUTTON;

	el->buffer = malloc(w * h * sizeof(uint32_t));
	el->text = strdup(text);

	el->draw = button_draw;
	el->event = button_event;

	btn->state = BUTTON_NORMAL;
	btn->on_click = NULL;
	btn->user_data = NULL;

	button_draw(el);

	return btn;
}
