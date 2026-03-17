#pragma once

#ifdef GUI_DEMO
#include "platform/keyboard.h"
#else

#include <gui/core/element/element.h>

typedef struct
{
	element_t *focused_el;
} local_keyboard_t;

extern local_keyboard_t g_keyboard;

void keyboard_update();

#endif
