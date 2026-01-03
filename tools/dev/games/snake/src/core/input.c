#include "input.h"
#include <ncurses.h>

bool input_poll_exit(void)
{
	int ch;
	while ((ch = getch()) != ERR)
	{
		if (ch == 27 || ch == 'q' || ch == 'Q')
			return true;
	}
	return false;
}
