#include "app.h"
#include "ui/menu_def.h"
#include "core/menu_stack.h"

int main()
{
	App app = {0};
	int selected_game = 0;

	app_init(&app);

	menu_push(&app, &main_menu);
	app.state = STATE_MENU;

	while (app.state != STATE_EXIT)
	{
		if (app.state == STATE_MENU)
			app.state = menu_run(&app);
	}

	app_shutdown(&app);
	return 0;
}
