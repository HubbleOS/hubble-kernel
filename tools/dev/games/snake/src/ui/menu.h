// ui/menu.h
#pragma once
#include "app.h"

typedef struct MenuItem
{
	const char *label;
	AppState (*action)(App *);
} MenuItem;

typedef struct Menu
{
	const char *title;
	MenuItem *items;
	int count;
} Menu;

AppState menu_run(App *app);
