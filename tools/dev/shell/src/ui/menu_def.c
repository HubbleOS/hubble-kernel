#include "menu_def.h"

#include "core/menu_stack.h"
#include "ui/menu.h"

#include "games/game.h"
#include "games/tetris/menu.h"
#include "games/snake/menu.h"

#include <misc.h>
#include <stdlib.h>

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

void run_demo()
{
	endwin();
	system("make -C ../../../  demo");
}

void run_build()
{
	endwin();
	system("make -C ../../../  build");
}

void run_run()
{
	endwin();
	system("make -C ../../../  run ");

	system("clear");
	fflush(stdout);

	initscr();
	refresh();
}

void run_disk()
{
	endwin();
	system("make -C ../../../  disk");
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
    {"Run Kernel", "Run the kernel", NULL, MENU_ITEM_ACTION, run_run},
    {"Run Build", "Build the kernel", NULL, MENU_ITEM_ACTION, run_build},
    {"Run Disk", "Create a disk image", NULL, MENU_ITEM_ACTION, run_disk},
    {"Run Demo", "Build and start the development GUI simulator", NULL, MENU_ITEM_ACTION, run_demo},
    {"Games", NULL, NULL, MENU_ITEM_ACTION, open_games_menu},
    {"Help", "Show Help-Menu", NULL, MENU_ITEM_ACTION, open_help_menu},
    {"Exit", NULL, NULL, MENU_ITEM_EXIT, NULL},
};

Menu main_menu = {
    "MAIN MENU",
    main_items,
    SIZE_OF_ARRAY(main_items)};
