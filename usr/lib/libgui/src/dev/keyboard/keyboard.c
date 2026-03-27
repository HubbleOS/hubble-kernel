// #ifndef GUI_DEMO

// #include "keyboard.h"
// #include <dev/keyboard.h> // ← key_event_t, keyboard_poll_event, keymap_lookup_char

// #include <core/object/object.h>
// #include <core/element/element.h>

// local_keyboard_t g_keyboard = {0};

// bool keyboard_poll_input(input_event_t *input)
// {
// 	key_event_t evt;

// 	if (!keyboard_poll_event(&evt))
// 		return false;

// 	if (evt.released)
// 		return false;

// 	input->shift = evt.is_shift;
// 	input->ctrl = evt.is_ctrl;
// 	input->alt = evt.is_alt;

// 	char c = keymap_lookup_char(
// 	    evt.id.scancode,
// 	    evt.id.extended,
// 	    evt.is_shift,
// 	    evt.is_caps_lock);

// 	if (c == '\b')
// 	{
// 		input->type = KEY_TYPE_SPECIAL;
// 		input->action = KEY_ACTION_BACKSPACE;
// 		return true;
// 	}
// 	if (c == '\n')
// 	{
// 		input->type = KEY_TYPE_SPECIAL;
// 		input->action = KEY_ACTION_ENTER;
// 		return true;
// 	}
// 	if (c == '\t')
// 	{
// 		input->type = KEY_TYPE_SPECIAL;
// 		input->action = KEY_ACTION_TAB;
// 		return true;
// 	}

// 	if (c)
// 	{
// 		input->type = KEY_TYPE_CHAR;
// 		input->character = c;
// 		return true;
// 	}

// 	return false;
// }

// void keyboard_update(void)
// {
// 	input_event_t ev;
// 	while (keyboard_poll_input(&ev))
// 	{
// 		element_t *el = g_keyboard.focused_el;
// 		if (!el)
// 			continue;

// 		if (ev.type == KEY_TYPE_CHAR && el->on_key_char)
// 			el->on_key_char(el, ev.character);

// 		if (ev.type == KEY_TYPE_SPECIAL && el->on_key_special)
// 			el->on_key_special(el, ev.action);
// 	}
// }

// #endif // GUI_DEMO
