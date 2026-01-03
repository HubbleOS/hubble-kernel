#pragma once
#include <ncurses.h>

typedef enum
{
	STATE_MENU,
	STATE_GAME,
	STATE_EXIT
} AppState;

typedef enum
{
	MODE_CLASSIC,
} GameMode;

typedef struct
{
	AppState state;
	GameMode mode;

	WINDOW *game_win;
	WINDOW *menu_win;

	int win_w, win_h;
} App;

void app_init(App *app);
void app_shutdown(App *app);
