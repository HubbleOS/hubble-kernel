#include "../element.h"
#include "../text/text.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

element_t *label_create(int x, int y, const char *text, uint32_t color)
{
	element_t *el = malloc(sizeof(element_t));
	if (!el)
		return NULL;

	element_init(el);

	int w = strlen(text) * 8;
	int h = 8;

	el->x = x;
	el->y = y;
	el->width = w;
	el->height = h;
	el->type = UI_LABEL;
	el->text = strdup(text);

	el->buffer = malloc(w * h * sizeof(uint32_t));
	if (!el->buffer)
	{
		free(el);
		return NULL;
	}

	// прозрачный фон (или нужный цвет)
	for (int i = 0; i < w * h; i++)
		el->buffer[i] = rgb(180, 180, 180); // фон окна

	// рисуем текст
	text_t *txt = element_create_text(0, 0, text);
	if (txt)
	{
		for (int row = 0; row < txt->height; row++)
			for (int col = 0; col < txt->width; col++)
			{
				uint32_t px = txt->buffer[row * txt->width + col];
				if (px == rgb(255, 255, 255))
					el->buffer[row * w + col] = color;
			}
		free(txt->buffer);
		free(txt->text);
		free(txt);
	}

	// label не реагирует на мышь
	el->on_mouse_enter = NULL;
	el->on_mouse_leave = NULL;
	el->on_mouse_down = NULL;
	el->on_mouse_up = NULL;

	return el;
}
