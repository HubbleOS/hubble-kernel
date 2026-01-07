#pragma once
#include <ncurses.h>

typedef enum
{
	STATE_NONE,
	STATE_MENU,
	STATE_EXIT
} AppState;

#define MENU_STACK_MAX 8

typedef struct Menu Menu;

typedef struct
{
	Menu *menu_stack[MENU_STACK_MAX];
	int menu_top;

	AppState state;
	WINDOW *win;
	WINDOW *hint_win;
	int win_w, win_h;
	int win_x, win_y;
} App;

void app_init(App *app);
void app_shutdown(App *app);
