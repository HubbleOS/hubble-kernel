#include "button.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

button_t *button_create(int x, int y, int w, int h, const char *text)
{
	button_t *btn = malloc(sizeof(button_t));
	if (!btn)
		return NULL;

	element_init(btn);

	btn->x = x;
	btn->y = y;
	btn->width = w;
	btn->height = h;
	btn->type = UI_BUTTON;

	btn->buffer = malloc(w * h * sizeof(uint32_t));

	if (!btn->buffer)
	{
		free(btn);
		return NULL;
	}

	btn->text = strdup(text);
	btn->on_click = NULL;

	btn->needs_redraw = true;
	return btn;
}
