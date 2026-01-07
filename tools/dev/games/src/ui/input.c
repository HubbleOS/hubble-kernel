#include "input.h"

InputAction input_get_action(WINDOW *win)
{
	int ch = wgetch(win);

	switch (ch)
	{
	case KEY_UP:
		return INPUT_UP;
	case KEY_DOWN:
		return INPUT_DOWN;
	case KEY_ENTER:
	case '\n':
	case ' ':
		return INPUT_SELECT;
	case KEY_BACKSPACE:
	case 127:
	case 8:
		return INPUT_BACK;
	case 27: // ESC
		return INPUT_HELP;
	case 'q':
		return INPUT_EXIT;
	default:
		return INPUT_NONE;
	}
}
