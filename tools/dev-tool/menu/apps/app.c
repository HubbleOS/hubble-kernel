#include "app.h"
#include <ui/main.h>
#include <ui/text.h>
#include <ui/button.h>
#include <string.h>
#include <stdlib.h>

#define MIN_WIDTH 80
#define MIN_HEIGHT 24

// ─────────────────────────────────────────────────────────────────────────────
// Terminal size warning

void show_terminal_size_warning_window()
{
	UI_CLEAR();

	int w = 40, h = 10;
	int x = (COLS - w) / 2;
	int y = (LINES - h) / 2;

	UIWindow *win = CREATE_WIN(h, w, y, x, "Warning");
	UIElement *label = text_create("Hello, World!", 2, 2);
	ADD_ELEMENT(win, label);
	DRAW_WIN(win);

	while (1)
	{
		int ch = getch();
		if (ch == '\n' || ch == 27 || ch == KEY_ENTER)
			break;
	}

	DESTROY_WIN(win);
}

// ─────────────────────────────────────────────────────────────────────────────
// Modal example actions

void on_save() { /* save logic */ }
void on_dont_save() { /* don't save logic */ }
void fun() { system("make -C qemu"); } // example action

void show_save_modal()
{
	ModalButton buttons[] = {
		{"Save", fun},
		{"Don't save", on_dont_save},
	};
	show_modal_with_buttons("Save changes?", buttons, COUNT(buttons));
}

void show_save_modal2()
{
	ModalButton buttons[] = {
		{"Save", fun},
		{"Save", fun},
		{"Save", fun},
		{"Save", fun},
		{"Don't save", on_dont_save},
	};
	show_modal_with_buttons("bla bla bla?", buttons, COUNT(buttons));
}

void start_game()
{
	system("gcc games/snake.c -o games/snake -lncurses && ./games/snake");
}

// ─────────────────────────────────────────────────────────────────────────────
// Menu setup

static MenuItem main_items[] = {
	{"Option 1", show_save_modal},
	{"Option 2", show_save_modal2},
	{"Option 3", fun},
	{"Option 4", NULL},
	{"Games :)", start_game},
};

static ChecklistItem checklist_items[] = {
	{"item123", true},
	{"item123", true},
	{"item123", true},
	{"item123", true},
	{"item123", true},
	{"item2", false},
	{"item3", false},
	{"item1", true},
	{"item2", false},
	{"item3", false},
	{"item1", true},
	{"item1", true},
	{"item2", false},
	{"item3", false},
	{"item1", true},
	{"item2", false},
	{"item3", false},
	{"item1", true},
	{"item2", false},
	{"item3", false},
	{"item1", true},
	{"item2", false},
	{"item3", false},
	{"item1", true},
	{"item2", false},
	{"item3", false},
	{"item4", false},
};

// ─────────────────────────────────────────────────────────────────────────────
// App lifecycle

static void draw_windows(UIWindow **windows, size_t count)
{
	for (int i = 0; i < count; i++)
	{
		if (windows[i])
			DRAW_WIN(windows[i]);
	}
}

static void destroy_windows(UIWindow **windows, size_t count)
{
	for (int i = 0; i < count; i++)
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
		break;
	default:
		if (*focused)
			WIN_HANDLE_KEY(*focused, ch);
		break;
	}
}

#define DRAW_WINDOWS(windows) draw_windows(windows, COUNT(windows))
#define DESTROY_WINDOWS(windows) destroy_windows(windows, COUNT(windows))

void app()
{
	int cols = COLS, lines = LINES;

	UIWindow *win_left = CREATE_WIN(lines, cols * 0.35, 0, 0, "Main Menu");
	UIWindow *win_right = CREATE_WIN(lines, cols - (cols * 0.35), 0, cols * 0.35, "Output");

	UIElement *main_menu = MAKE_MENU(ACTION_MENU, "Main Menu", main_items);
	UIElement *checklist = MAKE_MENU(CHECKLIST_MENU, "Checklist", checklist_items);

	ADD_ELEMENT(win_left, main_menu);
	ADD_ELEMENT(win_right, checklist);

	UIWindow *windows[] = {
		win_left,
		win_right,
	};

	UI_CLEAR();

	UIWindow *focused = win_left;
	SET_FOCUS(focused);

	DRAW_WINDOWS(windows);

	int ch;
	while ((ch = getch()) != 27)
	{ // ESC to quit
		handle_app_keypress(ch, &focused, windows);
		DRAW_WINDOWS(windows);
	}

	DESTROY_WINDOWS(windows);
}
