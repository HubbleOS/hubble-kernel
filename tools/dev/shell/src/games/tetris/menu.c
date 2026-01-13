#include "tetris.h"

#include "ui/menu.h"
#include "core/action.h"
#include "misc.h"

static MenuItem items[] = {
    {"Classic", NULL, NULL, MENU_ITEM_ACTION, tetris_classic_run},
    {"Back", NULL, NULL, MENU_ITEM_BACK, NULL}};

static Menu menu = {
    "TETRIS",
    items,
    SIZE_OF_ARRAY(items)};

Menu create_tetris_menu(void)
{
	return menu;
}
