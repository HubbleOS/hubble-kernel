#pragma once

typedef enum
{
	GAME_MODE_CLASSIC,
} GameMode;

typedef enum
{
	GAME_STATE_RUN,
	GAME_STATE_PAUSE,
	GAME_STATE_EXIT,
} GameState;

#include "../app.h"
#include <ncurses.h>

typedef struct
{
	const char *name;
	GameMode mode;

	int win_w, win_h;
	int win_x, win_y;

	void (*run)(void);
	// void (*draw)(void);
	void (*draw)(WINDOW *win);
	void (*reset)(void);

} Game;

extern Game tetris;
void tetris_init(void);

extern Game snake;
void snake_init(void);
