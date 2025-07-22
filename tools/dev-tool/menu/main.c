#include <ncurses.h>
#include <stdlib.h>
#include <signal.h>
#include "apps/app.h"

void end_scr()
{
	endwin();
	printf("Goodbye!\n");
}

void handle_sigint(int sig)
{
	end_scr();
	printf("handle_sigint!\n");
	exit(0);
}

void init_scr()
{
	initscr();
	set_escdelay(25);
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
	atexit(end_scr);
	signal(SIGINT, handle_sigint); // Ctrl+C
}

int main()
{
	init_scr();
	app();
	return 0;
}
