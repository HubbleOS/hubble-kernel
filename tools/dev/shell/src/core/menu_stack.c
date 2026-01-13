#include "app.h"
#include "ui/menu.h"
#include "menu_stack.h"

Menu *menu_stack[MENU_STACK_MAX];
int menu_top;

void menu_push(Menu *menu)
{
	if (menu_top < MENU_STACK_MAX)
		menu_stack[menu_top++] = menu;
}

bool menu_is_in_stack(Menu *menu)
{
	for (int i = 0; i < menu_top; i++)
	{
		if (menu_stack[i] == menu)
			return true;
	}
	return false;
}

bool menu_push_unique(Menu *menu)
{
	if (menu_is_in_stack(menu))
		return false;

	menu_push(menu);
	return true;
}

void menu_pop()
{
	if (menu_top > 1)
		menu_top--;
}

Menu *menu_current()
{
	if (menu_top == 0)
		return NULL;
	return menu_stack[menu_top - 1];
}
