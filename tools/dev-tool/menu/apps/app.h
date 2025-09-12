#pragma once

#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ui/menu.h>
#include <ui/main.h>

typedef struct
{
	void (*init)(void);
	void (*run)(void);
} App;

extern App app;
