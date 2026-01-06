#include <ui/menu.h>
#include <ui/main.h>
#include <stdlib.h>

typedef struct
{
	void *items;
	size_t item_size;
	size_t count;
	int highlight;
	int scroll;
	DrawItemFn draw_item;
	OnSelectFn on_select;
} ListMenuData;

static void update_scroll(ListMenuData *d, int visible_rows)
{
	if (d->highlight < d->scroll)
	{
		d->scroll = d->highlight;
	}
	else if (d->highlight >= d->scroll + visible_rows)
	{
		d->scroll = d->highlight - visible_rows + 1;
	}
}

static void list_menu_draw(UIElement *elem, WINDOW *win)
{
	ListMenuData *d = (ListMenuData *)elem->data;

	const int padding = 4;
	int height = getmaxy(win) - padding;
	int width = getmaxx(win) - padding;
	int visible_rows = height;

	bool is_focused = (elem->parent == WIN_GET_FOCUSED());

	update_scroll(d, visible_rows);

	for (int i = 0; i < visible_rows && (i + d->scroll) < (int)d->count; i++)
	{
		int item_index = i + d->scroll;
		int y = i + 2; // leave top margin
		void *item_ptr = (char *)d->items + item_index * d->item_size;

		bool is_selected = (is_focused && item_index == d->highlight);
		if (is_selected)
			wattron(win, A_REVERSE);

		mvwhline(win, y, 2, ' ', width); // clear line
		d->draw_item(win, item_ptr, y, width);

		if (is_selected)
			wattroff(win, A_REVERSE);
	}
}

static bool list_menu_handle_key(UIElement *elem, int ch)
{
	ListMenuData *d = (ListMenuData *)elem->data;

	switch (ch)
	{
	case KEY_UP:
		d->highlight = (d->highlight - 1 + d->count) % d->count;
		return true;
	case KEY_DOWN:
		d->highlight = (d->highlight + 1) % d->count;
		return true;
	case '\n':
	case ' ':
	case KEY_ENTER:
		if (d->on_select)
		{
			void *item = (char *)d->items + d->highlight * d->item_size;
			d->on_select(item);
		}
		return true;
	default:
		return false;
	}
}

static void list_menu_destroy(UIElement *elem)
{
	if (!elem)
		return;
	free(elem->data);
	free(elem);
}

UIElement *list_menu_create(void *items, size_t item_size, size_t count, DrawItemFn draw_item, OnSelectFn on_select)
{
	UIElement *elem = malloc(sizeof(UIElement));
	if (!elem)
		return NULL;

	ListMenuData *data = malloc(sizeof(ListMenuData));
	if (!data)
	{
		free(elem);
		return NULL;
	}

	*data = (ListMenuData){
	    .items = items,
	    .item_size = item_size,
	    .count = count,
	    .highlight = 0,
	    .scroll = 0,
	    .draw_item = draw_item,
	    .on_select = on_select,
	};

	elem->data = data;
	elem->draw = list_menu_draw;
	elem->handle_key = list_menu_handle_key;
	elem->destroy = list_menu_destroy;

	return elem;
}

UIElement *make_menu(MenuType type, const char *title, void *items, size_t count)
{
	(void)title; // Currently unused

	switch (type)
	{
	case ACTION_MENU:
		return action_menu_create(items, count);
	case CHECKLIST_MENU:
		return checklist_menu_create(items, count);
	default:
		return NULL;
	}
}
