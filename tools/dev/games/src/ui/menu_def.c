#include "menu_def.h"
#include "core/action_list.h"

#include "core/menu_stack.h"
#include "ui/menu.h"

#include "snake/snake.h"
#include "tetris/tetris.h"

#include <misc.h>

Menu games_menu;
Menu snake_menu;
Menu tetris_menu;

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

AppState open_tetris_menu(App *app)
{
	(void)app;
	menu_push(app, &tetris_menu);
	return STATE_MENU;
}

MenuItem tetris_items[] = {
    {"Classic", tetris_classic_run},
    {"Back", back}};

Menu tetris_menu = {
    "TETRIS",
    tetris_items,
    2};

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

MenuItem snake_items[] = {
    {"Classic", snake_classic_run},
    {"Hardcore", snake_hardcore_run},
    {"Back", back}};

Menu snake_menu = {
    "SNAKE",
    snake_items,
    3};
