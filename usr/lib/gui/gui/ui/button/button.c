#include "button.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

button_t *button_create(int x, int y, int w, int h, const char *text)
{
	element_t *el = element_create(x, y, w, h);
	if (!el)
		return NULL;

	el->type = UI_BUTTON;

	if (text)
	{
		el->text = strdup(text);
	}

	el->needs_redraw = true;

	return el;
}

void button_destroy(button_t *btn)
{
	element_destroy(btn);
}
