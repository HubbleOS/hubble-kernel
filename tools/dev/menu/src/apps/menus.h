#pragma once
#include <ui/menu.h>
#include "app.h"

extern MenuItem main_items[];

typedef struct
{
	MenuItem *items;
	size_t count;
} Menu;

extern Menu main_menu;

typedef struct
{
	ChecklistItem *items;
	size_t count;
} Checklist;

extern Checklist checklists;

void show_games_menu(void);
void show_save_modal(void);
void show_save_modal2(void);
