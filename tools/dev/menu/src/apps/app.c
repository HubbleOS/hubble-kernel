#include <string.h>
#include <stdlib.h>
#include "app.h"
#include "menus.h"
#include "warnings.h"
#include <ui/main.h>
#include <ui/text.h>
#include <ui/button.h>
#include <config/config.h>

// ─────────────────────────────────────────────────────────────────────────────
// App lifecycle

static void draw_windows(UIWindow **windows, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		if (windows[i])
			DRAW_WIN(windows[i]);
	}
}

static void destroy_windows(UIWindow **windows, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		if (windows[i])
			DESTROY_WIN(windows[i]);
	}
}

static void set_focus_window(UIWindow *win, UIWindow **focused)
{
	SET_FOCUS(win);
	*focused = win;
}

static void remove_focus_window(UIWindow **focused)
{
	REMOVE_FOCUS();
	*focused = NULL;
}

static void handle_app_keypress(int ch, UIWindow **focused, UIWindow **windows)
{
	switch (ch)
	{
	case 's':
		show_save_modal();
		break;
	case 'o':
		set_focus_window(windows[1], focused);
		break;
	case 'm':
		set_focus_window(windows[0], focused);
		break;
	case 'i':
		show_terminal_size_warning_window();
		break;
	case 'q':
		remove_focus_window(focused);
		*focused = ui_get_focused_window();
		break;
	default:
		if (*focused)
			WIN_HANDLE_KEY(*focused, ch);
		break;
	}
}

#define DRAW_WINDOWS(windows) draw_windows(windows, COUNT(windows))
#define DESTROY_WINDOWS(windows) destroy_windows(windows, COUNT(windows))

#include "screen.h"

static void run_app()
{
	int cols = COLS, lines = LINES;

	UIWindow *win_left = CREATE_WIN(lines, cols * 0.35, 0, 0, "Main Menu");
	UIWindow *win_right = CREATE_WIN(lines, cols - (cols * 0.35), 0, cols * 0.35, "Output");

	UIElement *main_menu_el = MAKE_MENU(ACTION_MENU, "Main Menu", main_menu.items, main_menu.count);
	UIElement *checklist_el = MAKE_MENU(CHECKLIST_MENU, "Checklist", checklists.items, checklists.count);

	ADD_ELEMENT(win_left, main_menu_el);
	ADD_ELEMENT(win_right, checklist_el);

	UIWindow *windows[] = {
	    win_left,
	    win_right,
	};

	screen.flush();

	UIWindow *focused = win_left;
	SET_FOCUS(focused);

	DRAW_WINDOWS(windows);

	int ch;
	while ((ch = getch()) != 27)
	{ // ESC to quit
		handle_app_keypress(ch, &focused, windows);
		DRAW_WINDOWS(windows);
	}

	config.save("../../../.config");

	DESTROY_WINDOWS(windows);
}

static void app_init()
{
	config.load("../../../.config");
	screen.init();
}

App app = {
    .run = run_app,
    .init = app_init,
};
