#include "tetris.h"
#include <app.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "core/window.h"

#include "ui/menu.h"
#include "games/game.h"

#include "menu.h"

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
			window_destroy(win);
			return STATE_MENU;
		}

		tetris_move();

		if (tetris_is_game_over())
		{
			window_destroy(win);
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

Game tetris = {
    .name = "Tetris",
    .mode = GAME_MODE_CLASSIC,
    .win_w = 10,
    .win_h = 20,
    .win_x = 0,
    .win_y = 0,
    .run = NULL,
    .draw = NULL,
    .reset = NULL,
};

Menu tetris_menu;

void tetris_init(void)
{
	tetris.run = tetris_classic_run;
	tetris.draw = tetris_draw;
	tetris.reset = tetris_reset;

	tetris_menu = create_tetris_menu();
}
