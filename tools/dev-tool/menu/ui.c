#include "ui.h"
#include "menu.h"
#include "utils.h"

static void draw_action_item(WINDOW *win, int y, int x, int width, void *item, bool highlighted)
{
	MenuItem *mi = (MenuItem *)item;
	mvwprintw(win, y, x, "%.*s", width, mi->label);
}

static void draw_checklist_item(WINDOW *win, int y, int x, int width, void *item, bool highlighted)
{
	ChecklistItem *ci = (ChecklistItem *)item;
	char mark = ci->checked ? '+' : ' ';
	mvwprintw(win, y, x, "[%c] %.*s", mark, width - 4, ci->label);
}

static void draw_menu_generic(WINDOW *win, void *items, int count, int hl, int scroll, int px, int py, int item_size, void (*draw_fn)(WINDOW *, int, int, int, void *, bool))
{
	int height = getmaxy(win) - py - 2;
	int width = getmaxx(win) - px - 4;

	for (int i = 0; i < height && (i + scroll) < count; i++)
	{
		int idx = i + scroll, y = i + py;
		if (idx == hl)
			wattron(win, A_REVERSE);

		mvwhline(win, y, px, ' ', width);
		draw_fn(win, y, px, width, (char *)items + idx * item_size, idx == hl);

		if (idx == hl)
			wattroff(win, A_REVERSE);
	}
}

static void draw_action_menu(WINDOW *win, void *items, int count, int hl, int scroll, int px, int py)
{
	draw_menu_generic(win, items, count, hl, scroll, px, py, sizeof(MenuItem), draw_action_item);
}

static void draw_checklist_menu(WINDOW *win, void *items, int count, int hl, int scroll, int px, int py)
{
	draw_menu_generic(win, items, count, hl, scroll, px, py, sizeof(ChecklistItem), draw_checklist_item);
}

static void on_action_enter(int idx, void *items)
{
	MenuItem *mi = (MenuItem *)items;
	if (mi[idx].action)
		mi[idx].action();
}

static void toggle_checklist_item(int idx, void *items)
{
	ChecklistItem *ci = (ChecklistItem *)items;
	ci[idx].checked = !ci[idx].checked;
}

Menu make_action_menu(const char *title, void *items, size_t count)
{
	return (Menu){items, count, draw_action_menu, title, on_action_enter};
}

Menu make_checklist_menu(const char *title, void *items, size_t count)
{
	return (Menu){items, count, draw_checklist_menu, title, toggle_checklist_item};
}

Menu make_menu(MenuType type, const char *title, void *items, size_t count)
{
	switch (type)
	{
	case ACTION_MENU:
		return make_action_menu(title, items, count);
	case CHECKLIST_MENU:
		return make_checklist_menu(title, items, count);
	default:
		return (Menu){0};
	}
}

//////////////////////////////////////////////////////////////////////////////
#include <string.h>

void draw_frame(WINDOW *win, const char *title)
{
	box(win, 0, 0);
	if (title)
	{
		// int x = 1;
		int x = (getmaxx(win) - (int)strlen(title)) / 2;
		mvwprintw(win, 0, x > 1 ? x : 1, " %s ", title);
	}
}

void draw_menu(const Menu *menu, WINDOW *win, int hl, int *scroll_offset, int px, int py)
{
	int max_visible = getmaxy(win) - py - 2;
	if (hl < *scroll_offset)
		*scroll_offset = hl;
	if (hl >= *scroll_offset + max_visible)
		*scroll_offset = hl - max_visible + 1;
	menu->draw(win, menu->items, (int)menu->count, hl, *scroll_offset, px, py);
}

void show_resize_warning(int tw, int th, int reqw, int reqh)
{
	int w = 40, h = 7;
	int x = (tw - w) / 2, y = (th - h) / 2;
	WINDOW *win = newwin(h, w, y, x);
	box(win, 0, 0);

	char msg2[64], msg4[64];
	snprintf(msg2, sizeof msg2, "  Width = %d Height = %d", tw, th);
	snprintf(msg4, sizeof msg4, "  Width = %d Height = %d", reqw, reqh);

	mvwprintw(win, 1, (w - 26) / 2, "Terminal size too small:");
	mvwprintw(win, 2, (w - (int)strlen(msg2)) / 2, "%s", msg2);
	mvwprintw(win, 4, (w - 26) / 2, "Needed for current config:");
	mvwprintw(win, 5, (w - (int)strlen(msg4)) / 2, "%s", msg4);

	wrefresh(win);
	delwin(win);
}

void handle_keypress(Menu *menu, int ch, int *hl, WINDOW *win_right)
{
	switch (ch)
	{
	case KEY_UP:
		*hl = (*hl - 1 + (int)menu->count) % (int)menu->count;
		break;
	case KEY_DOWN:
		*hl = (*hl + 1) % (int)menu->count;
		break;
	case '\n':
	case KEY_ENTER:
	case ' ':
		if (menu->action)
			menu->action(*hl, menu->items);
		break;
	case 27:
		break;
	default:
		break;
	}
}

#include <signal.h>
#include <unistd.h>

volatile sig_atomic_t resized = 0;
void on_resize(int sig)
{
	(void)sig;
	resized = 1;
}

extern WINDOW *g_win_left, *g_win_right;

#define win_left g_win_left
#define win_right g_win_right

void menu_loop(Menu *menu)
{
	int hl = 0, ch = ERR, px = 4, py = 2, scroll_offset = 0;
	const int MIN_WIDTH = 80, MIN_HEIGHT = 24;
	struct sigaction sa = {.sa_handler = on_resize, .sa_flags = SA_RESTART};
	sigaction(SIGWINCH, &sa, NULL);

	while (1)
	{
		if (resized)
		{
			endwin();
			refresh();
			clear();
			resized = 0;
			if (win_left)
				delwin(win_left);
			if (win_right)
				delwin(win_right);
			win_left = win_right = NULL;
		}

		if (!win_left || !win_right)
		{
			int lw = COLS * 0.35;
			win_left = newwin(LINES, lw, 0, 0);
			win_right = newwin(LINES, COLS - lw, 0, lw);
			keypad(win_left, TRUE);
			nodelay(win_left, TRUE);
		}

		if (COLS < MIN_WIDTH || LINES < MIN_HEIGHT)
		{
			show_resize_warning(COLS, LINES, MIN_WIDTH, MIN_HEIGHT);
			usleep(100000); // 100ms
			continue;
		}

		werase(win_left);
		werase(win_right);

		draw_frame(win_left, menu->title);

		draw_menu(menu, win_left, hl, &scroll_offset, px, py);

		draw_frame(win_right, "Output");

		wrefresh(win_right);
		wrefresh(win_left);

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		struct timeval tv = {0, 100000};
		int ret = select(1, &fds, NULL, NULL, &tv);
		ch = (ret > 0 && FD_ISSET(0, &fds)) ? wgetch(win_left) : ERR;

		//////////////////////////////////////////////////////////////////////////////
		if (ch == 27)
			return;
		wrefresh(win_right);

		// if (ch == '\b' || ch == KEY_BACKSPACE || ch == 127)
		// return;
		//////////////////////////////////////////////////////////////////////////////
		handle_keypress(menu, ch, &hl, win_right);
	}

	if (win_left)
		delwin(win_left);
	if (win_right)
		delwin(win_right);
	clear();
	refresh();
}
