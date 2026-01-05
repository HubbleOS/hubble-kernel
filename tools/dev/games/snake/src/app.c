#include "app.h"
#include "core/window.h"

void app_init(App *app)
{
	window_init(app);
	app->state = STATE_MENU;
}

void app_shutdown(App *app)
{
	delwin(app->game_win);
	delwin(app->menu_win);
	endwin();
}
