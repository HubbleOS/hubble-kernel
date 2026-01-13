#pragma once

#include "app.h"

typedef struct
{
	const char *name;
	AppState (*run)(App *app);
} Action;

AppState back(App *app);
AppState exit_app(App *app);
