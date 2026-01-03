#include "app.h"
#include "ui/menu.h"
#include "snake/snake.h"

int main()
{
	App app = {0};
	app_init(&app);

	while (app.state != STATE_EXIT)
	{
		if (app.state == STATE_MENU)
			app.state = menu_run(&app);
		else if (app.state == STATE_GAME)
			app.state = snake_classic_run(&app);
	}

	app_shutdown(&app);
	return 0;
}
