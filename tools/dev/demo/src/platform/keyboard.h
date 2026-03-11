#pragma once

#include <stdbool.h>
#include <stdint.h>

// #include <gui/core/element/element.h>
struct element;

typedef enum
{
	KEY_TYPE_CHAR,
	KEY_TYPE_SPECIAL,
	KEY_TYPE_FUNCTION,
	KEY_TYPE_MODIFIER,
	KEY_TYPE_UNKNOWN
} key_type_t;

typedef enum
{
	KEY_ACTION_NONE = 0,
	KEY_ACTION_UP,
	KEY_ACTION_DOWN,
	KEY_ACTION_LEFT,
	KEY_ACTION_RIGHT,
	KEY_ACTION_HOME,
	KEY_ACTION_END,
	KEY_ACTION_INSERT,
	KEY_ACTION_DELETE,
	KEY_ACTION_PAGE_UP,
	KEY_ACTION_PAGE_DOWN,
	KEY_ACTION_BACKSPACE,
	KEY_ACTION_ENTER,
	KEY_ACTION_TAB,
	KEY_ACTION_ESC,
} key_action_t;

typedef struct
{
	key_type_t type;
	union
	{
		char character;
		key_action_t action;
		uint8_t function_key;
	};
	bool shift;
	bool ctrl;
	bool alt;
} input_event_t;

typedef struct
{
	// element_t *focused_el;
	struct element *focused_el;
} local_keyboard_t;

extern local_keyboard_t g_keyboard;

bool keyboard_poll_input(input_event_t *input);
void keyboard_update(void);
void keyboard_queue_push(input_event_t ev);
