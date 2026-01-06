#include "app.h"
#include "ui/menu.h"

void menu_push(App *app, Menu *menu);

bool menu_is_in_stack(App *app, Menu *menu);
bool menu_push_unique(App *app, Menu *menu);

void menu_pop(App *app);

Menu *menu_current(App *app);
