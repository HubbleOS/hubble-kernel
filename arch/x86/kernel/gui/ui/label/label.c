#include "label.h"

#include "../text/text.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

element_t *label_create(int x, int y, const char *text, uint32_t color)
{
	element_t *el = element_create(x, y, 0, 0);
	if (!el)
		return NULL;

	int w = strlen(text) * 8;
	int h = 8;

	el->width = w;
	el->height = h;
	el->type = UI_LABEL;
	el->text = strdup(text);

	el->on_mouse_enter = NULL;
	el->on_mouse_leave = NULL;
	el->on_mouse_down = NULL;
	el->on_mouse_up = NULL;

	return el;
}

void label_destroy(element_t *el)
{
	element_destroy(el);
}
