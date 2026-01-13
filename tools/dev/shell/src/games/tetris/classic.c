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

void tetris_classic_run()
{
	tetris_reset();

	start_color();
	use_default_colors();
	init_pair(1, COLOR_CYAN, -1);	 // для I
	init_pair(2, COLOR_YELLOW, -1);	 // для O
	init_pair(3, COLOR_MAGENTA, -1); // для T
	init_pair(4, COLOR_GREEN, -1);	 // для S
	init_pair(5, COLOR_RED, -1);	 // для Z
	init_pair(6, COLOR_BLUE, -1);	 // для J
	init_pair(7, COLOR_WHITE, -1);	 // для L

	int base_delay = 100;
	int min_delay = 100;
	int delay_ms = base_delay;
	int prev_score = -1;

	nodelay(stdscr, TRUE);

	int th, tw;
	getmaxyx(stdscr, th, tw);
	WINDOW *win = newwin(th - 3, tw, 0, 0);

	while (1)
	{
		if (tetris_handle_input())
		{
			window_destroy(win);
			return;
		}

		tetris_move();

		if (tetris_is_game_over())
		{
			window_destroy(win);
			return;
		}

		werase(win);
		box(win, 0, 0);

		tetris_draw(win);

		int score = tetris_get_score();
		char score_str[32];
		snprintf(score_str, sizeof(score_str), " Score: %d ", score);
		mvprintw(0, (tw - (int)strlen(score_str)) / 2, "%s", score_str);

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
