#pragma once

#include "../element.h"

typedef enum
{
	BUTTON_NORMAL,
	BUTTON_HOVER,
	BUTTON_PRESSED,
} button_state_t;

typedef enum
{
	UI_EVENT_MOUSE_ENTER,
	UI_EVENT_MOUSE_LEAVE,
	UI_EVENT_MOUSE_DOWN,
	UI_EVENT_MOUSE_UP,
} ui_event_t;

typedef struct
{
	element_t base;

	button_state_t state;

	void (*on_click)(void *data);
	void *user_data;

} button_t;

button_t *button_create(int x, int y, int w, int h, const char *text);
