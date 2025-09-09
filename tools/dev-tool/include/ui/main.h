#pragma once

#include <ui/windows.h>
#include <ui/text.h>
#include <ui/button.h>
#include <ui/modal.h>
#include <ui/menu.h>

#include <ncurses.h>

#define UI_CLEAR()         \
	do                 \
	{                  \
		clear();   \
		refresh(); \
	} while (0)

#define CREATE_WIN(height, width, y, x, title) uiwindow_create(height, width, y, x, title)
#define DRAW_WIN(win) uiwindow_draw(win);
#define DESTROY_WIN(win) uiwindow_destroy(win)

#define ADD_ELEMENT(win, elem) uiwindow_add_element(win, elem);
#define GET_ELEMENT(win, idx) uiwindow_get_element(win, idx)

#define SET_FOCUS(win) ui_set_focused_window(win);
#define REMOVE_FOCUS() ui_remove_focus()
#define WIN_GET_FOCUSED() ui_get_focused_window()

#define WIN_HANDLE_KEY(win, ch) uiwindow_handle_key(win, ch)

#define COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
// #define MAKE_MENU(type, title, items) make_menu(type, title, items, COUNT(items))
#define MAKE_MENU(type, title, items, count) make_menu(type, title, items, count)
