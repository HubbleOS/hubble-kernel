#include <ncurses.h>
#include "menu.h"

int main()
{
	initscr();
	set_escdelay(25);
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);

	show_main_menu();

	endwin();
	return 0;
}
