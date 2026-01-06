#include "menu_def.h"

#include "core/menu_stack.h"
#include "ui/menu.h"

#include "games/game.h"
#include "games/tetris/menu.h"
#include "games/snake/menu.h"

#include <misc.h>

Menu games_menu;

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

AppState open_tetris_menu(App *app)
{
	(void)app;
	menu_push(app, &tetris_menu);
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
    {"Tetris", open_tetris_menu},
    {"Back", back}};

Menu games_menu = {
    "GAMES",
    games_items,
    SIZE_OF_ARRAY(games_items)};
