#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SIZE 15
#define PADDING 1
#define CELL_W 2

typedef enum
{
	UP,
	DOWN,
	LEFT,
	RIGHT,
	NONE
} Direction;

Direction buffered_dir = NONE;

const char *EMPTY = ". ";
const char *APPLE = "@ ";
const char *HEAD = "O ";
const char *BODY = "o ";

typedef struct
{
	int x, y;
} Point;

Point snake[SIZE * SIZE];
int snake_length = 3;
Point apple;
Point dir_offset[] = {
    [UP] = {0, -1},
    [DOWN] = {0, 1},
    [LEFT] = {-1, 0},
    [RIGHT] = {1, 0},
};
Direction current_dir = RIGHT;

int score = 0;

WINDOW *game_win;
int win_w, win_h;

void draw_cell(int y, int x, const char *symbol, int color_pair)
{
	wattron(game_win, COLOR_PAIR(color_pair));
	mvwprintw(game_win,
		  y + 1 + PADDING,
		  x * CELL_W + 1 + PADDING * CELL_W,
		  symbol);
	wattroff(game_win, COLOR_PAIR(color_pair));
}

bool points_equal(Point a, Point b)
{
	return a.x == b.x && a.y == b.y;
}

void place_apple()
{
	bool valid;
	do
	{
		valid = true;
		apple.x = rand() % SIZE;
		apple.y = rand() % SIZE;
		for (int i = 0; i < snake_length; i++)
		{
			if (points_equal(snake[i], apple))
			{
				valid = false;
				break;
			}
		}
	} while (!valid);
}

void draw_border_and_score()
{
	box(game_win, 0, 0);
	char score_str[32];
	snprintf(score_str, sizeof(score_str), " Score: %d ", score);
	mvwprintw(game_win, 0, (win_w - (int)strlen(score_str)) / 2, "%s", score_str);
}

void draw_snake_and_apple()
{
	for (int y = 0; y < SIZE; y++)
	{
		for (int x = 0; x < SIZE; x++)
		{
			bool drawn = false;

			if (points_equal(apple, (Point){x, y}))
			{
				draw_cell(y, x, APPLE, 1);
				drawn = true;
			}

			for (int i = 0; !drawn && i < snake_length; i++)
			{
				if (points_equal(snake[i], (Point){x, y}))
				{
					draw_cell(y, x, i == 0 ? HEAD : BODY, 2);
					drawn = true;
				}
			}

			if (!drawn)
				draw_cell(y, x, EMPTY, 0);
		}
	}
}

bool is_opposite(Direction a, Direction b)
{
	Point da = dir_offset[a];
	Point db = dir_offset[b];
	return da.x == -db.x && da.y == -db.y;
}

void handle_input()
{
	int ch;
	while ((ch = getch()) != ERR)
	{
		Direction d = NONE;

		switch (ch)
		{
		case KEY_UP:
		case 'w':
		case 'W':
			d = UP;
			break;
		case KEY_DOWN:
		case 's':
		case 'S':
			d = DOWN;
			break;
		case KEY_LEFT:
		case 'a':
		case 'A':
			d = LEFT;
			break;
		case KEY_RIGHT:
		case 'd':
		case 'D':
			d = RIGHT;
			break;
		case 'q':
		case 'Q':
			endwin();
			exit(0);
		}

		if (d == NONE)
			continue;

		/* 1️⃣ применяем сразу, если можно */
		if (!is_opposite(d, current_dir))
		{
			current_dir = d;
			return;
		}

		/* 2️⃣ иначе кладём в буфер (1 слот) */
		if (buffered_dir == NONE && !is_opposite(d, current_dir))
		{
			buffered_dir = d;
		}
	}
}

static bool check_collision(Point head, bool grow)
{
	int limit = grow ? snake_length : snake_length - 1;
	for (int i = 0; i < limit; i++)
		if (points_equal(snake[i], head))
			return true;
	return false;
}

static bool game_over = false;

void move_snake()
{
	Point delta = dir_offset[current_dir];
	Point new_head = {snake[0].x + delta.x, snake[0].y + delta.y};

	new_head.x = (new_head.x + SIZE) % SIZE;
	new_head.y = (new_head.y + SIZE) % SIZE;

	bool will_grow = (new_head.x == apple.x && new_head.y == apple.y);

	if (check_collision(new_head, will_grow))
	{
		nodelay(stdscr, FALSE);
		werase(game_win);
		box(game_win, 0, 0);
		mvwprintw(game_win, win_h / 2 - 1, (win_w - 9) / 2, "Game Over!");
		mvwprintw(game_win, win_h / 2, (win_w - 15) / 2, "Final score: %d", score);
		mvwprintw(game_win, win_h / 2 + 2, (win_w - 23) / 2, "Press any key to exit...");
		wrefresh(game_win);
		getch();

		game_over = true;
		return;
	}

	for (int i = snake_length; i > 0; i--)
		snake[i] = snake[i - 1];
	snake[0] = new_head;

	if (will_grow)
	{
		score += 10;
		snake_length++;
		place_apple();
	}
}

void apply_buffered_input()
{
	if (buffered_dir != NONE &&
	    !is_opposite(buffered_dir, current_dir))
	{
		current_dir = buffered_dir;
	}
	buffered_dir = NONE;
}

int main()
{
	srand(time(NULL));
	initscr();
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);

	start_color();
	use_default_colors();
	init_pair(1, COLOR_RED, -1);
	init_pair(2, COLOR_GREEN, -1);

	int term_h, term_w;
	getmaxyx(stdscr, term_h, term_w);

	win_w = (SIZE + PADDING * 2) * CELL_W + 2;
	win_h = SIZE + PADDING * 2 + 2;

	int start_y = (term_h - win_h) / 2;
	int start_x = (term_w - win_w) / 2;

	game_win = newwin(win_h, win_w, start_y, start_x);

	snake[0] = (Point){SIZE / 2, SIZE / 2};
	snake[1] = (Point){snake[0].x - 1, snake[0].y};
	snake[2] = (Point){snake[1].x - 1, snake[1].y};

	place_apple();

	int base_delay = 250;
	int min_delay = 150;
	int delay_ms = base_delay;
	int prev_score = -1;

	game_over = false;

	// while (!game_over)
	// {
	// 	handle_input();
	// 	apply_buffered_input();
	// 	move_snake();
	// 	if (game_over)
	// 		goto end;
	// 	werase(game_win);
	// 	draw_border_and_score();
	// 	draw_snake_and_apple();
	// 	wrefresh(game_win);

	// 	if (score != prev_score && delay_ms > min_delay)
	// 	{
	// 		delay_ms = base_delay - (score / 10) * 5;
	// 		if (delay_ms < min_delay)
	// 			delay_ms = min_delay;

	// 		prev_score = score;
	// 	}

	// 	napms(delay_ms);
	// }

	while (!game_over)
	{
		handle_input();
		apply_buffered_input();
		move_snake();
		if (game_over)
			goto end;

		werase(game_win);
		draw_border_and_score();
		draw_snake_and_apple();
		wrefresh(game_win);

		/* speed logic */
		napms(delay_ms);
	}

end:
	delwin(game_win);
	return 0;
}
