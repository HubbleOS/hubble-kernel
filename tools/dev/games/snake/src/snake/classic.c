#include "snake.h"
#include "../app.h"
#include <ncurses.h>

#include <snake/common.h>

AppState snake_classic_run(App *app)
{
	snake_reset();

	int base_delay = 250;
	int min_delay = 150;
	int delay_ms = base_delay;
	int prev_score = -1;

	nodelay(stdscr, TRUE);

	while (1)
	{
		if (snake_handle_input())
			return STATE_MENU;

		snake_apply_buffered_input();
		snake_move();

		if (snake_is_game_over())
			return STATE_MENU;

		werase(app->game_win);
		box(app->game_win, 0, 0);

		snake_draw(app->game_win);

		// Score
		int score = snake_get_score();
		char score_str[32];
		snprintf(score_str, sizeof(score_str), " Score: %d ", score);
		mvwprintw(app->game_win, 0, (app->win_w - (int)strlen(score_str)) / 2, "%s", score_str);

		wrefresh(app->game_win);

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
