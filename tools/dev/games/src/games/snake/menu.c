#include "snake.h"

#include "ui/menu.h"
#include "core/action.h"
#include "misc.h"

static MenuItem items[] = {
    {"Classic", snake_classic_run},
    {"Hardcore", snake_hardcore_run},
    {"Back", back}};

static Menu menu = {
    "SNAKE",
    items,
    SIZE_OF_ARRAY(items)};

Menu create_snake_menu(void)
{
	return menu;
}
