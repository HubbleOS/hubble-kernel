#pragma once

#include <ncurses.h>
#include <stdbool.h>
#include <stddef.h>
#include "menu.h"

typedef void (*DrawFn)(WINDOW *, void *, int, int, int, int, int);
typedef void (*ActionFn)(int idx, void *items);

typedef struct Menu
{
	void *items;
	size_t count;
	DrawFn draw;
	const char *title;
	ActionFn action;
} Menu;

Menu make_menu(MenuType type, const char *title, void *items, size_t count);
void menu_loop(Menu *menu);
