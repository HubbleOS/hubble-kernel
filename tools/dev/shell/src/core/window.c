#include "window.h"
#include <ncurses.h>

#define SIZE 15
#define PADDING 1
#define CELL_W 2

void window_init(App *app)
{
	initscr();
	set_escdelay(0);
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);

	int th, tw;
	getmaxyx(stdscr, th, tw);

	app->win_w = (SIZE + PADDING * 2) * CELL_W + 2;
	app->win_h = SIZE + PADDING * 2 + 2;

	int hint_w = 30; // width of hint window
	int y = (th - app->win_h) / 2;
	int x = (tw - app->win_w - hint_w - 2) / 2;

	app->win_x = x;
	app->win_y = y;

	// main-window
	app->win = newwin(app->win_h, app->win_w, app->win_y, app->win_x);
	keypad(app->win, TRUE);

	// hint-window
	app->hint_win = newwin(app->win_h, hint_w, app->win_y, app->win_x + app->win_w + 2);
	box(app->hint_win, 0, 0);
	wrefresh(app->hint_win);
}

void window_destroy(WINDOW *win)
{
	werase(win);
	wrefresh(win);
	delwin(win);
	touchwin(stdscr);
	wrefresh(stdscr);
}
