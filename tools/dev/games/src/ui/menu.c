#include "menu.h"
#include <string.h>
#include <ncurses.h>

#include "core/action.h"
#include "core/menu_stack.h"

#include "misc.h"

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

MenuItem help_items[] = {
    {"Back", back},
    {"Help", NULL},
    {"Exit", exit_app},
};

Menu help_menu = {
    "HELP",
    help_items,
    SIZE_OF_ARRAY(help_items)};

AppState open_help_menu(App *app)
{
	(void)app;
	menu_push(app, &help_menu);
	return STATE_MENU;
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

		case KEY_ENTER:
		case '\n':
		case ' ':
			if (menu->items[sel].action)
				return menu->items[sel].action(app);

		case KEY_BACKSPACE:
		case 127:
		case 8:
			return back(app);

		case 27: // ESC
			return open_help_menu(app);

		case 'q':
			return exit_app(app);
		}
	}
}
