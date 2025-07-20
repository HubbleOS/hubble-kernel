#include <ui/menu.h>

static void draw_action_item(WINDOW *win, void *item_ptr, int idx, int width)
{
	MenuItem *item = (MenuItem *)item_ptr;
	mvwprintw(win, idx, 2, "%.*s", width, item->label);
}

static void on_action_select(void *item_ptr)
{
	MenuItem *item = (MenuItem *)item_ptr;
	if (item->action)
		item->action();
}

UIElement *action_menu_create(MenuItem *items, size_t count)
{
	return list_menu_create(
		items, sizeof(MenuItem), count,
		draw_action_item, on_action_select);
}
