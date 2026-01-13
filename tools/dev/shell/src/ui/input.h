#pragma once

#include <ncurses.h>

typedef enum
{
	INPUT_NONE,
	INPUT_UP,
	INPUT_DOWN,
	INPUT_SELECT,
	INPUT_BACK,
	INPUT_EXIT,
	INPUT_HELP
} InputAction;

InputAction input_get_action(WINDOW *win);
