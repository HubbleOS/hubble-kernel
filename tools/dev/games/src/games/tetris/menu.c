#include "tetris.h"

#include "ui/menu.h"
#include "core/action.h"
#include "misc.h"

static MenuItem items[] = {
    {"Classic", tetris_classic_run},
    {"Back", back}};

static Menu menu = {
    "TETRIS",
    items,
    SIZE_OF_ARRAY(items)};

Menu create_tetris_menu(void)
{
	return menu;
}
