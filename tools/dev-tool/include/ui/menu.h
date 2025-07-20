#pragma once

#include <ui/windows.h>
#include <stdbool.h>

typedef struct
{
	const char *label;
	void (*action)(void);
} MenuItem;

typedef struct
{
	const char *label;
	bool checked;
} ChecklistItem;

typedef enum
{
	ACTION_MENU,
	CHECKLIST_MENU
} MenuType;

UIElement *checklist_menu_create(ChecklistItem *items, size_t count);
UIElement *action_menu_create(MenuItem *items, size_t count);

typedef void (*DrawItemFn)(WINDOW *, void *item, int idx, int width);
typedef void (*OnSelectFn)(void *item);

UIElement *list_menu_create(void *items, size_t item_size, size_t count, DrawItemFn draw_item, OnSelectFn on_select);

UIElement *make_menu(MenuType type, const char *title, void *items, size_t count);
