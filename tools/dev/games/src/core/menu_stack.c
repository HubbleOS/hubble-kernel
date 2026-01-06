#include "app.h"
#include "ui/menu.h"
#include "menu_stack.h"

void menu_push(App *app, Menu *menu)
{
	if (app->menu_top < MENU_STACK_MAX)
		app->menu_stack[app->menu_top++] = menu;
}

bool menu_is_in_stack(App *app, Menu *menu)
{
	for (int i = 0; i < app->menu_top; i++)
	{
		if (app->menu_stack[i] == menu)
			return true;
	}
	return false;
}

bool menu_push_unique(App *app, Menu *menu)
{
	if (menu_is_in_stack(app, menu))
		return false;

	menu_push(app, menu);
	return true;
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
