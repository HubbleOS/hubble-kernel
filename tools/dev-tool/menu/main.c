#include <ncurses.h>
#include "apps/app.h"

void init_scr()
{
	initscr();
	set_escdelay(25);
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
}

void end_scr()
{
	endwin();
}

int main()
{
	init_scr();
	app();
	end_scr();
	return 0;
}
