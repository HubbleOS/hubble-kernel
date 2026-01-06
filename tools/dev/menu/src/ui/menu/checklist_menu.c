#include <ui/menu.h>
#include <stdlib.h>

static void draw_checklist_item(WINDOW *win, void *item_ptr, int idx, int width)
{
	ChecklistItem *item = (ChecklistItem *)item_ptr;
	char mark = item->checked ? '+' : ' ';
	mvwprintw(win, idx, 2, "[%c] %.*s", mark, width - 4, item->label);
}

static void on_checklist_toggle(void *item_ptr)
{
	ChecklistItem *item = (ChecklistItem *)item_ptr;
	item->checked = !item->checked;
}

UIElement *checklist_menu_create(ChecklistItem *items, size_t count)
{
	return list_menu_create(
	    items,
	    sizeof(ChecklistItem),
	    count,
	    draw_checklist_item,
	    on_checklist_toggle);
}
