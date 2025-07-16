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