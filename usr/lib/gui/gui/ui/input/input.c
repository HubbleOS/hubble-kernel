#include "input.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <gui/core/element/element.h>

static void input_key_char(element_t *el, char c)
{
	size_t len = strlen(el->text);

	char *new = malloc(len + 2);
	strcpy(new, el->text);

	new[len] = c;
	new[len + 1] = 0;

	free(el->text);
	el->text = new;

	element_redraw(el);
}

// static void input_key_special(element_t *el, key_action_t action)
static void input_key_special(element_t *el, int action)
{
	if (!el->text)
		return;

	// if (action == KEY_ACTION_BACKSPACE)
	// {
	// 	size_t len = strlen(el->text);
	// 	if (len == 0)
	// 		return;

	// 	el->text[len - 1] = 0;
	// 	element_redraw(el);
	// }
}

input_t *input_create(int x, int y, int w, int h, const char *text)
{
	element_t *el = element_create(x, y, w, h);
	if (!el)
		return NULL;

	el->type = UI_TEXTBOX;

	el->on_key_char = input_key_char;
	el->on_key_special = input_key_special;

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
