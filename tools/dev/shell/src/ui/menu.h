// ui/menu.h
#pragma once
#include "app.h"

typedef enum
{
	MENU_ITEM_ACTION,
	MENU_ITEM_SUBMENU,
	MENU_ITEM_BACK,
	MENU_ITEM_EXIT
} MenuItemType;

typedef struct MenuItem
{
	const char *label;
	const char *hint;
	const char *cmd;

	MenuItemType type;

	void (*action)(void);
} MenuItem;

typedef struct Menu
{
	const char *title;
	MenuItem *items;
	size_t count;
} Menu;

AppState menu_run(App *app);

void open_help_menu();
