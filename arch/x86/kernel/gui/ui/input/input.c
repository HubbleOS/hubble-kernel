#include "input.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

input_t *input_create(int x, int y, int w, int h, const char *text)
{
	element_t *el = element_create(x, y, w, h);
	if (!el)
		return NULL;

	// el->type = UI_BUTTON;

	if (text)
	{
		el->text = strdup(text);
	}

	el->needs_redraw = true;

	return el;
}

void input_destroy(input_t *input)
{
	element_destroy(input);
}
