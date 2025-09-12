#pragma once

#include <ncurses.h>

typedef struct
{
	void (*init)();
	void (*end)();
} Screen;

extern Screen screen;
