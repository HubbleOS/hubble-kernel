#include "tetris.h"
#include "../app.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

AppState tetris_classic_run(App *app)
{
	tetris_reset();

	int base_delay = 150;
	int min_delay = 100;
	int delay_ms = base_delay;
	int prev_score = -1;

	nodelay(stdscr, TRUE);

	WINDOW *win = newwin(app->win_h + 3, app->win_w, app->win_y, app->win_x);

	while (1)
	{
		if (tetris_handle_input())
		{
			delwin(win);
			return STATE_MENU;
		}

		tetris_move();

		if (tetris_is_game_over())
		{
			delwin(win);
			return STATE_MENU;
		}

		werase(win);
		box(win, 0, 0);

		tetris_draw(win);

		int score = tetris_get_score();
		char score_str[32];
		snprintf(score_str, sizeof(score_str), " Score: %d ", score);
		mvwprintw(win, 0, (app->win_w - (int)strlen(score_str)) / 2, "%s", score_str);

		wrefresh(win);
		napms(delay_ms);
	}
}
