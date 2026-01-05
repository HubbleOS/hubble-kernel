// tetris/common.c
#include "tetris.h"
#include "../app.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define WIDTH 10
#define HEIGHT 20
#define BLOCK_SIZE 4

static int field[HEIGHT][WIDTH];
static int cur_piece, rotation;
static int pos_x, pos_y;
static bool game_over;
static int score;

/* Tetrominoes definitions */
static const int tetrominoes[7][4][4][4] = {
    // I
    {{{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}},
     {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}}},
    // O
    {{{0, 0, 0, 0}, {0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
     {{0, 0, 0, 0}, {0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
     {{0, 0, 0, 0}, {0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
     {{0, 0, 0, 0}, {0, 1, 1, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}}},
    // T
    {{{0, 0, 0, 0}, {1, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {0, 1, 1, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}}},
    // S
    {{{0, 0, 0, 0}, {0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}},
     {{1, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
     {{0, 0, 0, 0}, {0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}},
     {{1, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}}},
    // Z
    {{{0, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {1, 1, 0, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {1, 1, 0, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}}},
    // J
    {{{0, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {0, 1, 0, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}},
     {{1, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 1, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}}},
    // L
    {{{0, 0, 0, 0}, {1, 1, 1, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}},
     {{1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 0, 0}, {0, 0, 0, 0}},
     {{0, 0, 1, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
     {{0, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}}}};

/* ----------------- Helpers ----------------- */
static bool check_collision(int nx, int ny, int r)
{
	for (int y = 0; y < BLOCK_SIZE; y++)
		for (int x = 0; x < BLOCK_SIZE; x++)
			if (tetrominoes[cur_piece][r][y][x])
			{
				int fx = nx + x;
				int fy = ny + y;
				if (fx < 0 || fx >= WIDTH || fy >= HEIGHT || (fy >= 0 && field[fy][fx]))
					return true;
			}
	return false;
}

static void place_piece()
{
	for (int y = 0; y < BLOCK_SIZE; y++)
		for (int x = 0; x < BLOCK_SIZE; x++)
			if (tetrominoes[cur_piece][rotation][y][x])
			{
				field[pos_y + y][pos_x + x] = cur_piece + 1;
			}
}

static void clear_lines()
{
	for (int y = HEIGHT - 1; y >= 0; y--)
	{
		int filled = 1;
		for (int x = 0; x < WIDTH; x++)
			if (!field[y][x])
				filled = 0;
		if (filled)
		{
			for (int row = y; row > 0; row--)
				memcpy(field[row], field[row - 1], sizeof(field[0]));
			memset(field[0], 0, sizeof(field[0]));
			y++;
			score += 10;
		}
	}
}

/* ----------------- API ----------------- */
void tetris_reset()
{
	memset(field, 0, sizeof(field));
	score = 0;
	game_over = false;
	cur_piece = rand() % 7;
	rotation = 0;
	pos_x = WIDTH / 2 - 2;
	pos_y = -2;
}

bool tetris_is_game_over() { return game_over; }
int tetris_get_score() { return score; }

int tetris_handle_input()
{
	int ch;
	while ((ch = getch()) != ERR)
	{
		switch (ch)
		{
		case KEY_LEFT:
			if (!check_collision(pos_x - 1, pos_y, rotation))
				pos_x--;
			break;
		case KEY_RIGHT:
			if (!check_collision(pos_x + 1, pos_y, rotation))
				pos_x++;
			break;
		case KEY_DOWN:
			if (!check_collision(pos_x, pos_y + 1, rotation))
				pos_y++;
			break;
		case ' ':
			if (!check_collision(pos_x, pos_y, (rotation + 1) % 4))
				rotation = (rotation + 1) % 4;
			break;
		case 27:
		case 'q':
			return 1;
		}
	}
	return 0;
}

void tetris_move()
{
	static int tick = 0;
	tick++;
	if (tick >= 5) // can parameterize as speed
	{
		tick = 0;
		if (!check_collision(pos_x, pos_y + 1, rotation))
			pos_y++;
		else
		{
			place_piece();
			clear_lines();
			cur_piece = rand() % 7;
			rotation = 0;
			pos_x = WIDTH / 2 - 2;
			pos_y = -2;
			if (check_collision(pos_x, pos_y, rotation))
				game_over = true;
		}
	}
}

void tetris_draw(WINDOW *win)
{
	for (int y = 0; y < HEIGHT; y++)
	{
		for (int x = 0; x < WIDTH; x++)
		{
			mvwprintw(win, y + 1, x * 2 + 1, field[y][x] ? "[]" : " .");
		}
	}

	for (int y = 0; y < BLOCK_SIZE; y++)
	{
		for (int x = 0; x < BLOCK_SIZE; x++)
		{
			if (tetrominoes[cur_piece][rotation][y][x] && pos_y + y >= 0)
			{
				mvwprintw(win, pos_y + y + 1, (pos_x + x) * 2 + 1, "[]");
			}
		}
	}

	box(win, 0, 0);
}
