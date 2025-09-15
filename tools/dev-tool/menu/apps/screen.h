#pragma once

#include <ncurses.h>

typedef struct
{
	void (*init)(void);
	void (*flush)(void);
	void (*end)(void);
} Screen;

extern Screen screen;
