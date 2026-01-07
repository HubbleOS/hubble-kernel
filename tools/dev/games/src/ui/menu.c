#include "menu.h"
#include <string.h>
#include <ncurses.h>

#include "core/action.h"
#include "core/menu_stack.h"

#include "ui/input.h"
#include "ui/menu_def.h"

#include "misc.h"

static void draw_menu(App *app, Menu *menu, int sel)
{
	werase(app->win);
	box(app->win, 0, 0);

	mvwprintw(app->win, 2,
		  (app->win_w - strlen(menu->title)) / 2,
		  "%s", menu->title);

	for (int i = 0; i < menu->count; i++)
	{
		int y = 5 + i;
		mvwprintw(app->win, y, 2, "  %s  ", menu->items[i].label);

		if (i == sel)
			mvwchgat(app->win, y, 2, app->win_w - 4, A_REVERSE, 0, NULL);
	}

	wrefresh(app->win);

	// hint
	werase(app->hint_win);
	box(app->hint_win, 0, 0);

	if (menu->items[sel].hint)
	{
		mvwprintw(app->hint_win, 1, 2, "%s", menu->items[sel].hint);
	}

	wrefresh(app->hint_win);
}

AppState open_help_menu(App *app)
{
	menu_push_unique(app, &help_menu);
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

		switch (input_get_action(app->win))
		{
		case INPUT_UP:
			sel = (sel - 1 + menu->count) % menu->count;
			break;

		case INPUT_DOWN:
			sel = (sel + 1) % menu->count;
			break;

		case INPUT_SELECT:
			if (menu->items[sel].action)
				return menu->items[sel].action(app);
			if (menu->items[sel].action == NULL)
				return STATE_NONE;

		case INPUT_BACK:
			return back(app);

		case INPUT_HELP:
			return open_help_menu(app);

		case INPUT_EXIT:
			return exit_app(app);

		default:
			break;
		}
	}
}
