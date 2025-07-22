#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SIZE 15
#define PADDING 1
#define CELL_W 2

#define INPUT_BUFFER_SIZE 4

typedef enum
{
	UP,
	DOWN,
	LEFT,
	RIGHT,
	NONE
} Direction;

Direction input_buffer[INPUT_BUFFER_SIZE];
int input_buffer_len = 0;

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
int dx = 1, dy = 0;
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
			if (snake[i].x == apple.x && snake[i].y == apple.y)
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

			if (x == apple.x && y == apple.y)
			{
				draw_cell(y, x, APPLE, 1);
				drawn = true;
			}

			for (int i = 0; !drawn && i < snake_length; i++)
			{
				if (snake[i].x == x && snake[i].y == y)
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

void enqueue_direction(Direction dir)
{
	if (input_buffer_len < INPUT_BUFFER_SIZE)
		input_buffer[input_buffer_len++] = dir;
}

void handle_input()
{
	int ch;
	while ((ch = getch()) != ERR)
	{
		switch (ch)
		{
		case KEY_UP:
		case 'w':
		case 'W':
			enqueue_direction(UP);
			break;
		case KEY_DOWN:
		case 's':
		case 'S':
			enqueue_direction(DOWN);
			break;
		case KEY_LEFT:
		case 'a':
		case 'A':
			enqueue_direction(LEFT);
			break;
		case KEY_RIGHT:
		case 'd':
		case 'D':
			enqueue_direction(RIGHT);
			break;
		case 'q':
		case 'Q':
			endwin();
			exit(0);
		}
	}
}

void apply_buffered_input()
{
	for (int i = 0; i < input_buffer_len; i++)
	{
		Direction dir = input_buffer[i];
		switch (dir)
		{
		case UP:
			if (dy != 1)
			{
				dx = 0;
				dy = -1;
				goto done;
			}
			break;
		case DOWN:
			if (dy != -1)
			{
				dx = 0;
				dy = 1;
				goto done;
			}
			break;
		case LEFT:
			if (dx != 1)
			{
				dx = -1;
				dy = 0;
				goto done;
			}
			break;
		case RIGHT:
			if (dx != -1)
			{
				dx = 1;
				dy = 0;
				goto done;
			}
			break;
		default:
			break;
		}
	}

done:
	input_buffer_len = 0;
}

bool check_collision(Point head)
{
	for (int i = 0; i < snake_length; i++)
	{
		if (snake[i].x == head.x && snake[i].y == head.y)
			return true;
	}
	return false;
}

void move_snake()
{
	Point new_head = {snake[0].x + dx, snake[0].y + dy};

	if (new_head.x < 0)
		new_head.x = SIZE - 1;
	else if (new_head.x >= SIZE)
		new_head.x = 0;

	if (new_head.y < 0)
		new_head.y = SIZE - 1;
	else if (new_head.y >= SIZE)
		new_head.y = 0;

	if (check_collision(new_head))
	{
		nodelay(stdscr, FALSE);
		werase(game_win);
		box(game_win, 0, 0);
		mvwprintw(game_win, win_h / 2 - 1, (win_w - 9) / 2, "Game Over!");
		mvwprintw(game_win, win_h / 2, (win_w - 15) / 2, "Final score: %d", score);
		mvwprintw(game_win, win_h / 2 + 2, (win_w - 23) / 2, "Press any key to exit...");
		wrefresh(game_win);
		getch();
		endwin();
		exit(0);
	}

	for (int i = snake_length; i > 0; i--)
		snake[i] = snake[i - 1];
	snake[0] = new_head;

	if (new_head.x == apple.x && new_head.y == apple.y)
	{
		score += 10;
		snake_length++;
		place_apple();
	}
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
	init_pair(1, COLOR_RED, -1);   // Apple
	init_pair(2, COLOR_GREEN, -1); // Snake

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

	while (true)
	{
		handle_input();
		apply_buffered_input();
		move_snake();
		werase(game_win);
		draw_border_and_score();
		draw_snake_and_apple();
		wrefresh(game_win);

		if (score != prev_score && delay_ms > min_delay)
		{
			delay_ms = base_delay - (score / 10) * 5;
			if (delay_ms < min_delay)
				delay_ms = min_delay;

			prev_score = score;
		}

		mvprintw(0, 0, "Delay: %d", delay_ms);

		napms(delay_ms);
	}

	delwin(game_win);
	endwin();
	return 0;
}
