#include <ui/button.h>
#include <stdlib.h>
#include <string.h>

static void button_draw(UIElement *elem, WINDOW *win)
{
	UIButtonData *btn = (UIButtonData *)elem->data;
	if (!btn)
		return;

	if (btn->highlighted)
		wattron(win, A_REVERSE);
	mvwprintw(win, elem->y, elem->x, "[ %s ]", btn->label);
	if (btn->highlighted)
		wattroff(win, A_REVERSE);
}

static bool button_handle_key(UIElement *elem, int ch)
{
	UIButtonData *btn = (UIButtonData *)elem->data;
	if (!btn)
		return false;

	switch (ch)
	{
	case KEY_LEFT:
		btn->highlighted = false;
		break;
	case KEY_RIGHT:
		btn->highlighted = true;
		break;

	case '\n':
	case ' ':
	case KEY_ENTER:
		if (btn->action)
			btn->action();
		break;
	default:
		break;
	}

	return false;
}

UIElement *button_create(const char *label, UIButtonCallback action, int x, int y, int w, int h)
{
	UIElement *elem = malloc(sizeof(UIElement));
	UIButtonData *data = malloc(sizeof(UIButtonData));
	data->label = label;
	data->action = action;
	data->highlighted = false;

	elem->data = data;
	elem->x = x;
	elem->y = y;
	elem->w = w;
	elem->h = h;
	elem->draw = button_draw;
	elem->handle_key = button_handle_key;
	return elem;
}
