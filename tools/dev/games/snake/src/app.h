#pragma once
#include <ncurses.h>

typedef enum
{
	STATE_MENU,
	STATE_EXIT
} AppState;

typedef enum
{
	MODE_CLASSIC,
} GameMode;

#define MENU_STACK_MAX 8

typedef struct Menu Menu;

typedef struct
{
	Menu *menu_stack[MENU_STACK_MAX];
	int menu_top;

	AppState state;
	WINDOW *menu_win;
	WINDOW *game_win;
	int win_w, win_h;
} App;

void app_init(App *app);
void app_shutdown(App *app);
