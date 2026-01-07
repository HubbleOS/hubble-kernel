#include "snake.h"

#include "ui/menu.h"
#include "core/action.h"
#include "misc.h"

static MenuItem items[] = {
    {"Classic", "Start with 3 segments", snake_classic_run},
    {"Hardcore", "In development...", snake_hardcore_run},
    {"Back", NULL, back}};

static Menu menu = {
    "SNAKE",
    items,
    SIZE_OF_ARRAY(items)};

Menu create_snake_menu(void)
{
	return menu;
}
