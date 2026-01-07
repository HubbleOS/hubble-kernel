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

MenuItem games_items[] = {
    {"Snake", NULL, open_snake_menu},
    {"Tetris", NULL, open_tetris_menu},
    {"Back", NULL, back}};

Menu games_menu = {
    "GAMES",
    games_items,
    SIZE_OF_ARRAY(games_items)};

MenuItem help_items[] = {
    {"Back", NULL, back},
    {"Exit", NULL, exit_app},
};

Menu help_menu = {
    "HELP",
    help_items,
    SIZE_OF_ARRAY(help_items)};

MenuItem main_items[] = {
    {"Games", NULL, open_games_menu},
    {"Help", "Show Help-Menu", open_help_menu},
    {"Exit", NULL, exit_app}};

Menu main_menu = {
    "MAIN MENU",
    main_items,
    SIZE_OF_ARRAY(main_items)};
