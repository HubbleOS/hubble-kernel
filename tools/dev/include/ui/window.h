#pragma once

#include <ncurses.h>
#include <stdbool.h>

typedef struct UIWindow UIWindow;
typedef struct UIElement UIElement;

typedef void (*DestroyFn)(UIElement *elem);
typedef void (*DrawFn)(UIElement *elem, WINDOW *win);
typedef bool (*HandleKeyFn)(UIElement *elem, int ch);

struct UIElement
{
	DestroyFn destroy;
	DrawFn draw;
	HandleKeyFn handle_key;
	void *data;
	int x, y, w, h;
	UIWindow *parent;
};

struct UIWindow
{
	WINDOW *win;
	UIElement **elements;
	int element_count;
	int active_element;
	const char *title;
};

UIWindow *uiwindow_create(int height, int width, int y, int x, const char *title);
void uiwindow_draw(UIWindow *win);
void uiwindow_destroy(UIWindow *win);

void uiwindow_add_element(UIWindow *win, UIElement *elem);
UIElement *uiwindow_get_element(UIWindow *win, int index);
void draw_frame(UIWindow *win);

void ui_set_focused_window(UIWindow *win);
void ui_remove_focus();
UIWindow *ui_get_focused_window();

bool uiwindow_handle_key(UIWindow *win, int ch);
