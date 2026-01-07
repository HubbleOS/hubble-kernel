#include "snake.h"

#include "ui/menu.h"
#include "core/action.h"
#include "misc.h"

static MenuItem items[] = {
    {"Classic", "Start with 3 segments", NULL, MENU_ITEM_ACTION, snake_classic_run},
    {"Hardcore", "In development...", NULL, MENU_ITEM_ACTION, NULL},
    {"Back", NULL, NULL, MENU_ITEM_BACK, NULL}};

static Menu menu = {
    "SNAKE",
    items,
    SIZE_OF_ARRAY(items)};

Menu create_snake_menu(void)
{
	return menu;
}
