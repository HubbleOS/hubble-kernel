#pragma once

#include <gui/core/element/element.h>

typedef struct
{
	element_t *focused_el;
} local_keyboard_t;

extern local_keyboard_t g_keyboard;

void keyboard_update();
