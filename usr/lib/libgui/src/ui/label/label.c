#include "label.h"

#include <stdlib.h>
#include <string.h>

label_t *label_create(int x, int y, int w, int h, const char *text)
{
	element_t *el = element_create(x, y, w, h);
	if (!el)
		return NULL;

	el->type = UI_LABEL;
	el->bg_color = 0;
	el->text_color = rgb(0, 0, 0);

	if (text)
		el->text = strdup(text);

	el->needs_redraw = true;
	return el;
}

void label_destroy(label_t *label)
{
	element_destroy(label);
}

void label_set_text(label_t *label, const char *text)
{
	if (!label)
		return;

	if (label->text)
		free(label->text);

	label->text = text ? strdup(text) : NULL;
	label->needs_redraw = true;
}
