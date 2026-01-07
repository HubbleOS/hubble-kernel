// ui/menu.h
#pragma once
#include "app.h"

typedef struct MenuItem
{
	const char *label;
	const char *hint;
	AppState (*action)(App *);
} MenuItem;

typedef struct Menu
{
	const char *title;
	MenuItem *items;
	size_t count;
} Menu;

AppState menu_run(App *app);

AppState open_help_menu(App *app);
