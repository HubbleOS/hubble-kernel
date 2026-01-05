#include "tetris.h"

#include "ui/menu.h"
#include "core/action.h"
#include "misc.h"

static MenuItem items[] = {
    {"Classic", tetris_classic_run},
    {"Back", back}};

Menu create_tetris_menu(void)
{
	return creaate_menu("TETRIS", items);
}
