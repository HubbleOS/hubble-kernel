#include "snake.h"
#include <ncurses.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <app.h>
#include "common.h"

#define SIZE 15
#define CELL_W 2
#define PADDING 1

typedef enum
{
	UP,
	DOWN,
	LEFT,
	RIGHT,
	NONE
} Direction;

typedef struct
{
	int x, y;
} Point;

/* ----------------- Snake State ----------------- */
static Point snake[SIZE * SIZE];
static int snake_length;
static Point apple;
static Direction current_dir;
static Direction buffered_dir;
static int score;
static bool game_over;

/* ----------------- Helpers ----------------- */
static const char *EMPTY = ". ";
static const char *APPLE_SYM = "@ ";
static const char *HEAD = "O ";
static const char *BODY = "o ";

static Point dir_offset[] = {
    [UP] = {0, -1},
    [DOWN] = {0, 1},
    [LEFT] = {-1, 0},
    [RIGHT] = {1, 0},
};

static bool points_equal(Point a, Point b)
{
	return a.x == b.x && a.y == b.y;
}

static bool is_opposite(Direction a, Direction b)
{
	Point da = dir_offset[a];
	Point db = dir_offset[b];
	return da.x == -db.x && da.y == -db.y;
}

/* ----------------- Snake Logic ----------------- */
static void place_apple()
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

static bool check_collision(Point head, bool grow)
{
	int limit = grow ? snake_length : snake_length - 1;
	for (int i = 0; i < limit; i++)
		if (points_equal(snake[i], head))
			return true;
	return false;
}

/* ----------------- Input ----------------- */
int snake_handle_input()
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
		case 27:
		case 'q':
		case 'Q':
			return 1;
		}

		if (d == NONE)
			continue;

		if (d == current_dir)
			continue;

		if (!is_opposite(d, current_dir))
		{
			current_dir = d;
			return 0;
		}

		if (buffered_dir == NONE && !is_opposite(d, current_dir))
			buffered_dir = d;
	}
	return 0;
}

void snake_apply_buffered_input()
{
	if (buffered_dir != NONE && !is_opposite(buffered_dir, current_dir))
		current_dir = buffered_dir;
	buffered_dir = NONE;
}

/* ----------------- Move & Collision ----------------- */
void snake_move()
{
	Point delta = dir_offset[current_dir];
	Point new_head = {snake[0].x + delta.x, snake[0].y + delta.y};

	// wrap-around
	new_head.x = (new_head.x + SIZE) % SIZE;
	new_head.y = (new_head.y + SIZE) % SIZE;

	bool will_grow = points_equal(new_head, apple);

	if (check_collision(new_head, will_grow))
	{
		game_over = true;
		return;
	}

	// shift body
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

/* ----------------- Drawing ----------------- */
void snake_draw(WINDOW *win)
{
	for (int y = 0; y < SIZE; y++)
	{
		for (int x = 0; x < SIZE; x++)
		{
			bool drawn = false;

			if (points_equal(apple, (Point){x, y}))
			{
				wattron(win, COLOR_PAIR(1));
				mvwprintw(win, y + 1 + PADDING, x * CELL_W + 1 + PADDING * CELL_W, APPLE_SYM);
				wattroff(win, COLOR_PAIR(1));
				drawn = true;
			}

			for (int i = 0; !drawn && i < snake_length; i++)
			{
				if (points_equal(snake[i], (Point){x, y}))
				{
					wattron(win, COLOR_PAIR(2));
					mvwprintw(win, y + 1 + PADDING, x * CELL_W + 1 + PADDING * CELL_W,
						  i == 0 ? HEAD : BODY);
					wattroff(win, COLOR_PAIR(2));
					drawn = true;
				}
			}

			if (!drawn)
				mvwprintw(win, y + 1 + PADDING, x * CELL_W + 1 + PADDING * CELL_W, EMPTY);
		}
	}
}

/* ----------------- Reset ----------------- */
void snake_reset()
{
	snake_length = 3;
	snake[0] = (Point){SIZE / 2, SIZE / 2};
	snake[1] = (Point){snake[0].x - 1, snake[0].y};
	snake[2] = (Point){snake[1].x - 1, snake[1].y};

	current_dir = RIGHT;
	buffered_dir = NONE;
	score = 0;
	game_over = false;

	place_apple();
}

/* ----------------- Getters ----------------- */
bool snake_is_game_over() { return game_over; }
int snake_get_score() { return score; }
