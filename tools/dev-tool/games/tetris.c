#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define WIDTH 10
#define HEIGHT 20
#define BLOCK_SIZE 4
#define DELAY 300

int field[HEIGHT][WIDTH];

const int tetrominoes[7][4][4][4] = {
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

int cur_piece, rotation = 0;
int pos_x = WIDTH / 2 - 2, pos_y = 0;

bool check_collision(int nx, int ny, int r)
{
	for (int y = 0; y < BLOCK_SIZE; y++)
	{
		for (int x = 0; x < BLOCK_SIZE; x++)
		{
			if (tetrominoes[cur_piece][r][y][x])
			{
				int fx = nx + x;
				int fy = ny + y;
				if (fx < 0 || fx >= WIDTH || fy >= HEIGHT || (fy >= 0 && field[fy][fx]))
					return true;
			}
		}
	}
	return false;
}

void place_piece()
{
	for (int y = 0; y < BLOCK_SIZE; y++)
	{
		for (int x = 0; x < BLOCK_SIZE; x++)
		{
			if (tetrominoes[cur_piece][rotation][y][x])
			{
				field[pos_y + y][pos_x + x] = cur_piece + 1;
			}
		}
	}
}

void clear_lines()
{
	for (int y = HEIGHT - 1; y >= 0; y--)
	{
		int filled = 1;
		for (int x = 0; x < WIDTH; x++)
		{
			if (!field[y][x])
				filled = 0;
		}
		if (filled)
		{
			for (int row = y; row > 0; row--)
			{
				memcpy(field[row], field[row - 1], sizeof(field[0]));
			}
			memset(field[0], 0, sizeof(field[0]));
			y++; // recheck same row
		}
	}
}

void draw_field()
{
	for (int y = 0; y < HEIGHT; y++)
	{
		for (int x = 0; x < WIDTH; x++)
		{
			mvprintw(y + 1, x * 2 + 1, field[y][x] ? "[]" : " .");
		}
	}
	for (int y = 0; y < BLOCK_SIZE; y++)
	{
		for (int x = 0; x < BLOCK_SIZE; x++)
		{
			if (tetrominoes[cur_piece][rotation][y][x] && pos_y + y >= 0)
				mvprintw(pos_y + y + 1, (pos_x + x) * 2 + 1, "[]");
		}
	}
	box(stdscr, 0, 0);
}

void new_piece()
{
	cur_piece = rand() % 7;
	rotation = 0;
	pos_x = WIDTH / 2 - 2;
	pos_y = -2;
	if (check_collision(pos_x, pos_y, rotation))
	{
		endwin();
		printf("Game Over\n");
		exit(0);
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

	new_piece();

	int tick = 0;
	while (1)
	{
		clear();
		draw_field();
		refresh();

		int ch = getch();
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
		case 'q':
			endwin();
			return 0;
		}

		if (++tick >= DELAY / 10)
		{
			tick = 0;
			if (!check_collision(pos_x, pos_y + 1, rotation))
			{
				pos_y++;
			}
			else
			{
				place_piece();
				clear_lines();
				new_piece();
			}
		}

		napms(50);
	}

	endwin();
	return 0;
}
