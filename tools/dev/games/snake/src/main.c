#include "app.h"
#include "ui/menu.h"
#include "core/action_list.h"

#include "core/menu_stack.h"
#include "ui/menu.h"

#include "snake/snake.h"

Menu games_menu;
Menu snake_menu;

AppState open_games_menu(App *app)
{
	(void)app;
	menu_push(app, &games_menu);
	return STATE_MENU;
}

AppState open_snake_menu(App *app)
{
	(void)app;
	menu_push(app, &snake_menu);
	return STATE_MENU;
}

AppState snake_hardcore_run(App *app)
{
	(void)app;
	return STATE_MENU;
}

MenuItem main_items[] = {
    {"Games", open_games_menu},
    {"Exit", exit_app}};

Menu main_menu = {
    "MAIN MENU",
    main_items,
    2};

MenuItem games_items[] = {
    {"Snake", open_snake_menu},
    {"Back", back}};

Menu games_menu = {
    "GAMES",
    games_items,
    2};

MenuItem snake_items[] = {
    {"Classic", snake_classic_run},
    {"Hardcore", snake_hardcore_run},
    {"Back", back}};

Menu snake_menu = {
    "SNAKE",
    snake_items,
    3};

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
