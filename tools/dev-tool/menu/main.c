#include <ncurses.h>
#include <stdlib.h>
#include "apps/app.h"

void end_scr()
{
	endwin();
	printf("Terminal return to normal!\n");
}

void init_scr()
{
	initscr();
	set_escdelay(25);
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
	atexit(end_scr);
}

int main()
{
	init_scr();
	app();
	return 0;
}
