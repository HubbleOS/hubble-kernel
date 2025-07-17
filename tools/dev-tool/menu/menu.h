#pragma once

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

typedef struct Menu Menu;

void show_main_menu(void);
void show_make_menu(void);
void show_checklist(void);
