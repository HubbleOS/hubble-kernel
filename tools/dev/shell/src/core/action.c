#include "action.h"
#include "menu_stack.h"

AppState back(App *app)
{
	(void)app;
	menu_pop(app);
	return STATE_MENU;
}

AppState exit_app(App *app)
{
	(void)app;
	return STATE_EXIT;
}
