#include "menu.h"
#include <string.h>
#include <ncurses.h>

#include "core/action.h"
#include "core/menu_stack.h"

static void draw_menu(App *app, Menu *menu, int sel)
{
	werase(app->menu_win);
	box(app->menu_win, 0, 0);

	mvwprintw(app->menu_win, 2,
		  (app->win_w - strlen(menu->title)) / 2,
		  "%s", menu->title);

	for (int i = 0; i < menu->count; i++)
	{
		if (i == sel)
			wattron(app->menu_win, A_REVERSE);
		mvwprintw(app->menu_win, 5 + i * 2,
			  (app->win_w - strlen(menu->items[i].label)) / 2,
			  "%s",
			  menu->items[i].label);
		if (i == sel)
			wattroff(app->menu_win, A_REVERSE);
	}

	wrefresh(app->menu_win);
}

AppState menu_run(App *app)
{
	Menu *menu = menu_current(app);
	int sel = 0;

	nodelay(stdscr, FALSE);

	while (1)
	{
		draw_menu(app, menu, sel);

		int ch = wgetch(app->menu_win);
		switch (ch)
		{
		case KEY_UP:
			sel = (sel - 1 + menu->count) % menu->count;
			break;

		case KEY_DOWN:
			sel = (sel + 1) % menu->count;
			break;

		case '\n':
		case ' ':
			return menu->items[sel].action(app);

		case KEY_BACKSPACE:
		case 127:
		case 8:
			return back(app);

		case 27: // ESC
			return exit_app(app);
		}
	}
}
