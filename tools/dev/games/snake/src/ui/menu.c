#include "menu.h"
#include <string.h>

static void draw_menu(WINDOW *w, int sel, int width)
{
	werase(w);
	box(w, 0, 0);

	const char *items[] = {"Classic Snake", "Exit"};

	mvwprintw(w, 2, (width - 11) / 2, "S N A K E");

	for (int i = 0; i < 2; i++)
	{
		if (i == sel)
			wattron(w, A_REVERSE);
		mvwprintw(w, 5 + i * 2, (width - strlen(items[i])) / 2, "%s", items[i]);
		if (i == sel)
			wattroff(w, A_REVERSE);
	}

	wrefresh(w);
}

AppState menu_run(App *app)
{
	int sel = 0;
	nodelay(stdscr, FALSE);

	while (1)
	{
		draw_menu(app->menu_win, sel, app->win_w);
		int ch = wgetch(app->menu_win);

		switch (ch)
		{
		case KEY_UP:
			sel = (sel + 1) % 2;
			break;
		case KEY_DOWN:
			sel = (sel + 1) % 2;
			break;
		case '\n':
			return sel == 0 ? STATE_GAME : STATE_EXIT;
		case 27:
			return STATE_EXIT;
		}
	}
}
