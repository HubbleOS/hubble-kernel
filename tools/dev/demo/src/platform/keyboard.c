#include "keyboard.h"

#include <gui/core/element/element.h>

local_keyboard_t g_keyboard = {0};

#define KEY_QUEUE_SIZE 32
static input_event_t key_queue[KEY_QUEUE_SIZE];
static int key_queue_head = 0;
static int key_queue_tail = 0;

void keyboard_queue_push(input_event_t ev)
{
	int next = (key_queue_tail + 1) % KEY_QUEUE_SIZE;
	if (next != key_queue_head)
	{
		key_queue[key_queue_tail] = ev;
		key_queue_tail = next;
	}
}

bool keyboard_poll_input(input_event_t *input)
{
	if (key_queue_head == key_queue_tail)
		return false;
	*input = key_queue[key_queue_head];
	key_queue_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
	return true;
}

void keyboard_update(void)
{
	input_event_t ev;
	while (keyboard_poll_input(&ev))
	{
		element_t *el = g_keyboard.focused_el;
		if (!el)
			continue;

		if (ev.type == KEY_TYPE_CHAR && el->on_key_char)
			el->on_key_char(el, ev.character);

		if (ev.type == KEY_TYPE_SPECIAL && el->on_key_special)
			el->on_key_special(el, ev.action);
	}
}
