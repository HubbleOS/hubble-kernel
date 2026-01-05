#include "snake.h"

#include "ui/menu.h"
#include "core/action.h"
#include "misc.h"

static MenuItem items[] = {
    {"Classic", snake_classic_run},
    {"Hardcore", snake_hardcore_run},
    {"Back", back}};

Menu create_snake_menu(void)
{
	return creaate_menu("SNAKE", items);
}
