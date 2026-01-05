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

	start_color();
	use_default_colors();
	init_pair(1, COLOR_RED, -1);
	init_pair(2, COLOR_GREEN, -1);

	int th, tw;
	getmaxyx(stdscr, th, tw);

	app->win_w = (SIZE + PADDING * 2) * CELL_W + 2;
	app->win_h = SIZE + PADDING * 2 + 2;

	int y = (th - app->win_h) / 2;
	int x = (tw - app->win_w) / 2;

	app->win_x = x;
	app->win_y = y;

	app->menu_win = newwin(app->win_h, app->win_w, app->win_y, app->win_x);

	keypad(app->menu_win, TRUE);
}

void window_destroy(WINDOW *win)
{
	werase(win);
	wrefresh(win);
	delwin(win);
	touchwin(stdscr);
	wrefresh(stdscr);
}
