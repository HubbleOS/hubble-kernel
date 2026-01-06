#include "screen.h"
#include <stdio.h>
#include <stdlib.h>

static void screen_init()
{
	initscr();
	set_escdelay(25);
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
	atexit(screen.end);
}

static void screen_flush()
{
	clear();
	refresh();
}

static void screen_end()
{
	endwin();
	printf("Terminal return to normal!\n");
}

static void srceen_reset()
{
	printf("\033[0m");
	printf("\033[H\033[J");
	fflush(stdout);
}

Screen screen = {
    .init = screen_init,
    .flush = screen_flush,
    .end = screen_end,
    .reset = srceen_reset,
};
