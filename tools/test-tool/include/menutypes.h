// Example header
#pragma once

#include <ncurses.h>

typedef struct
{
	const char *label;
	void (*action)(WINDOW *output_win);
} MenuItem;

typedef struct
{
	const char *label;
	bool checked;
} ChecklistItem;
