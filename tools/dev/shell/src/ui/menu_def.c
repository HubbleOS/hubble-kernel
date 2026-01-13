#include "menu_def.h"

#include "core/menu_stack.h"
#include "ui/menu.h"

#include "games/game.h"
#include "games/tetris/menu.h"
#include "games/snake/menu.h"

#include <misc.h>

Menu games_menu;

void open_games_menu()
{
	menu_push(&games_menu);
}

void open_snake_menu()
{
	menu_push(&snake_menu);
}

void open_tetris_menu()
{
	menu_push(&tetris_menu);
}

MenuItem games_items[] = {
    {"Snake", NULL, NULL, MENU_ITEM_ACTION, open_snake_menu},
    {"Tetris", NULL, NULL, MENU_ITEM_ACTION, open_tetris_menu},
    {"Back", NULL, NULL, MENU_ITEM_BACK, NULL},

};

Menu games_menu = {
    "GAMES",
    games_items,
    SIZE_OF_ARRAY(games_items)};

MenuItem help_items[] = {
    {"Back", NULL, NULL, MENU_ITEM_BACK, NULL},
    {"Exit", NULL, NULL, MENU_ITEM_EXIT, NULL},
};

Menu help_menu = {
    "HELP",
    help_items,
    SIZE_OF_ARRAY(help_items)};

MenuItem main_items[] = {
    {"Games", NULL, NULL, MENU_ITEM_ACTION, open_games_menu},
    {"Help", "Show Help-Menu", NULL, MENU_ITEM_ACTION, open_help_menu},
    {"Exit", NULL, NULL, MENU_ITEM_EXIT, NULL},
};

Menu main_menu = {
    "MAIN MENU",
    main_items,
    SIZE_OF_ARRAY(main_items)};
