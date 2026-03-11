#include "keyboard.h"

#include <gui/core/object/object.h>
#include <gui/core/element/element.h>

local_keyboard_t g_keyboard = {0};

#include <dev/keyboard.h>

bool keyboard_poll_input(input_event_t *input)
{

#ifdef GUI_DEMO
	return false;
#endif

	key_event_t evt;

	if (!keyboard_poll_event(&evt))
		return false;

	if (evt.released)
		return false;

	input->shift = evt.is_shift;
	input->ctrl = evt.is_ctrl;
	input->alt = evt.is_alt;

	char c = keymap_lookup_char(
	    evt.id.scancode,
	    evt.id.extended,
	    evt.is_shift,
	    evt.is_caps_lock);

	if (c == '\b')
	{
		input->type = KEY_TYPE_SPECIAL;
		input->action = KEY_ACTION_BACKSPACE;
		return true;
	}

	if (c == '\n')
	{
		input->type = KEY_TYPE_SPECIAL;
		input->action = KEY_ACTION_ENTER;
		return true;
	}

	if (c == '\t')
	{
		input->type = KEY_TYPE_SPECIAL;
		input->action = KEY_ACTION_TAB;
		return true;
	}

	if (c)
	{
		input->type = KEY_TYPE_CHAR;
		input->character = c;
		return true;
	}

	return false;
}

void keyboard_update()
{
#ifdef GUI_DEMO
	return;
#endif

	input_event_t ev;

	while (keyboard_poll_input(&ev))
	{
		element_t *el = g_keyboard.focused_el;
		if (!el)
			continue;

		if (ev.type == KEY_TYPE_CHAR)
		{
			if (el->on_key_char)
				el->on_key_char(el, ev.character);
		}

		if (ev.type == KEY_TYPE_SPECIAL)
		{
			if (el->on_key_special)
				el->on_key_special(el, ev.action);
		}
	}
}
