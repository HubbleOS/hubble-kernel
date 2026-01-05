#include "app.h"
#include "ui/menu.h"
#include "menu_stack.h"

void menu_push(App *app, Menu *menu)
{
	if (app->menu_top < MENU_STACK_MAX)
		app->menu_stack[app->menu_top++] = menu;
}

void menu_pop(App *app)
{
	if (app->menu_top > 1)
		app->menu_top--;
}

Menu *menu_current(App *app)
{
	if (app->menu_top == 0)
		return NULL;
	return app->menu_stack[app->menu_top - 1];
}
