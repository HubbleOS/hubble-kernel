#include "snake.h"
#include <app.h>

#include <ncurses.h>

#include "common.h"

#include "core/window.h"

#include <games/game.h>

#include "ui/menu.h"
#include "menu.h"

void snake_classic_run()
{
	// werase(app->win);
	// wrefresh(app->win);

	// werase(app->hint_win);
	// wrefresh(app->hint_win);

	snake_reset();

	start_color();
	use_default_colors();
	init_pair(1, COLOR_RED, -1);
	init_pair(2, COLOR_GREEN, -1);

	int base_delay = 250;
	int min_delay = 150;
	int delay_ms = base_delay;
	int prev_score = -1;

	nodelay(stdscr, TRUE);

	int th, tw;
	getmaxyx(stdscr, th, tw);
	WINDOW *win = newwin(th - 3, tw, 0, 0);

	while (1)
	{
		if (snake_handle_input())
		{
			window_destroy(win);
			return;
		}

		snake_apply_buffered_input();
		snake_move();

		if (snake_is_game_over())
		{
			window_destroy(win);
			return;
		}

		werase(win);
		box(win, 0, 0);

		snake_draw(win);

		// Score
		int score = snake_get_score();
		char score_str[32];
		snprintf(score_str, sizeof(score_str), " Score: %d ", score);
		mvwprintw(win, 0, (tw - (int)strlen(score_str)) / 2, "%s", score_str);

		wrefresh(win);

		/* speed logic */
		if (score != prev_score && delay_ms > min_delay)
		{
			delay_ms = base_delay - (score / 10) * 5;
			if (delay_ms < min_delay)
				delay_ms = min_delay;

			prev_score = score;
		}

		napms(delay_ms);
	}
}

AppState snake_hardcore_run(App *app)
{
	(void)app;
	return STATE_MENU;
}

Game snake = {
    .name = "Snake",
    .mode = GAME_MODE_CLASSIC,
    .win_w = 15,
    .win_h = 15,
    .win_x = 0,
    .win_y = 0,
    .run = NULL,
    .draw = NULL,
    .reset = NULL,
};

Menu snake_menu;

void snake_init(void)
{
	snake.run = snake_classic_run;
	snake.draw = snake_draw;
	snake.reset = snake_reset;

	snake_menu = create_snake_menu();
}
