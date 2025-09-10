#include <stdlib.h>
#include <string.h>
#include <ui/window.h>
#include <ui/button.h>
#include <ui/main.h>

UIWindow *uiwindow_create(int height, int width, int y, int x, const char *title)
{
	UIWindow *u = malloc(sizeof(UIWindow));
	SET_FOCUS(u);
	u->win = newwin(height, width, y, x);
	u->elements = NULL;
	u->element_count = 0;
	u->active_element = 0;
	u->title = title;
	keypad(u->win, TRUE);
	return u;
}

void uiwindow_destroy(UIWindow *win)
{
	if (!win)
		return;

	REMOVE_FOCUS();
	for (int i = 0; i < win->element_count; i++)
	{
		UIElement *el = win->elements[i];
		if (el)
		{
			if (el->destroy)
			{
				el->destroy(el);
			}
			else
			{
				free(el);
			}
		}
	}

	free(win->elements);
	delwin(win->win);
	free(win);
}

void uiwindow_add_element(UIWindow *win, UIElement *elem)
{
	elem->parent = win;

	win->elements = realloc(win->elements, sizeof(UIElement *) * (win->element_count + 1));
	win->elements[win->element_count++] = elem;
}

void draw_frame(UIWindow *win)
{
	box(win->win, 0, 0);
	if (win->title)
	{
		int x = (getmaxx(win->win) - (int)strlen(win->title)) / 2;
		mvwprintw(win->win, 0, x > 1 ? x : 1, " %s ", win->title);
	}
}

void uiwindow_draw(UIWindow *win)
{
	werase(win->win);
	draw_frame(win);
	for (int i = 0; i < win->element_count; i++)
	{
		UIElement *el = win->elements[i];
		el->draw(el, win->win);
	}
	wrefresh(win->win);
}

bool uiwindow_handle_key(UIWindow *win, int ch)
{
	if (win->element_count == 0)
		return false;

	UIElement *active = win->elements[win->active_element];

	if (active->handle_key)
		return active->handle_key(active, ch);

	return false;
}

UIElement *uiwindow_get_element(UIWindow *win, int index)
{
	if (index >= 0 && index < win->element_count)
		return win->elements[index];
	return NULL;
}

static UIWindow *focused_window = NULL;

#define MAX_FOCUS_STACK 16

static UIWindow *focus_stack[MAX_FOCUS_STACK];
static int focus_stack_top = -1;

#include <string.h>

void ui_push_focused_window(UIWindow *win)
{
	if (WIN_GET_FOCUSED() == win)
		return;

	if (focus_stack_top < MAX_FOCUS_STACK - 1)
	{
		focus_stack[++focus_stack_top] = win;
	}
	else
	{
		memmove(&focus_stack[0], &focus_stack[1],
				sizeof(UIWindow *) * (MAX_FOCUS_STACK - 1));

		focus_stack[MAX_FOCUS_STACK - 1] = win;
	}
}

UIWindow *ui_pop_focused_window()
{
	if (focus_stack_top <= 0)
		return NULL;

	return focus_stack[focus_stack_top--];
}

UIWindow *ui_get_focused_window()
{
	return (focus_stack_top >= 0) ? focus_stack[focus_stack_top] : NULL;
}

void ui_set_focused_window(UIWindow *win)
{
	ui_push_focused_window(win);
}

void ui_remove_focus()
{
	ui_pop_focused_window();
}
