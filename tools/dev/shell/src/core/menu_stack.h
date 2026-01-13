#include "app.h"
#include "ui/menu.h"

void menu_push(Menu *menu);

bool menu_is_in_stack(Menu *menu);
bool menu_push_unique(Menu *menu);

void menu_pop();

Menu *menu_current();
