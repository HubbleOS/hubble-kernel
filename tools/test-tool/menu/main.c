#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/select.h>
#include <unistd.h>

// #include "menutypes.h"

typedef struct
{
	const char *label;
	void (*action)(void);
} MenuItem;

typedef struct
{
	const char *label;
	bool checked;
} ChecklistItem;

#define COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MAKE_MENU(type, title, items) make_menu(type, title, items, COUNT(items))

typedef struct Menu Menu;

typedef void (*DrawFn)(WINDOW *, void *, int, int, int, int, int);
typedef void (*Action)(int idx, void *items);
typedef void (*KeyHandler)(Menu *menu, int ch, int *hl, WINDOW *win_right);

typedef enum
{
	ACTION_MENU,
	CHECKLIST_MENU,
} MenuType;

struct Menu
{
	void *items;
	size_t count;
	DrawFn draw;
	const char *title;
	Action action;
	KeyHandler handle_key;
};

void draw_action_menu(WINDOW *, void *, int, int, int, int, int);
void draw_checklist_menu(WINDOW *, void *, int, int, int, int, int);
void on_action_enter(int, void *);
void toggle_checklist_item(int, void *);

Menu make_action_menu(const char *title, MenuItem *items, size_t count);
Menu make_checklist_menu(const char *title, ChecklistItem *items, size_t count);
Menu make_menu(MenuType type, const char *title, void *items, size_t count);

Menu make_action_menu(const char *title, MenuItem *items, size_t count)
{
	return (Menu){items, count, draw_action_menu, title, on_action_enter, NULL};
}

Menu make_checklist_menu(const char *title, ChecklistItem *items, size_t count)
{
	return (Menu){items, count, draw_checklist_menu, title, toggle_checklist_item, NULL};
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

volatile sig_atomic_t resized = 0;
void on_resize(int sig)
{
	(void)sig;
	resized = 1;
}

void strip_nonprintable(char *d, const char *s)
{
	while (*s)
	{
		if (isprint(*s) || *s == '\n' || *s == '\t')
			*d++ = *s;
		s++;
	}
	*d = '\0';
}

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

typedef void (*DrawItemFn)(WINDOW *win, int y, int x, int width, void *item, bool highlighted);

void draw_menu_generic(WINDOW *win, void *items, int count, int hl, int scroll, int px, int py, int item_size, DrawItemFn draw_item)
{
	int height = getmaxy(win) - py - 2;
	int width = getmaxx(win) - px - 4;

	for (int i = 0; i < height && (i + scroll) < count; i++)
	{
		int idx = i + scroll;
		int y = i + py;
		if (idx == hl)
			wattron(win, A_REVERSE);

		mvwhline(win, y, px, ' ', width);

		draw_item(win, y, px, width, (char *)items + idx * item_size, idx == hl);

		if (idx == hl)
			wattroff(win, A_REVERSE);
	}
}

void draw_action_item(WINDOW *win, int y, int x, int width, void *item, bool highlighted)
{
	MenuItem *mi = (MenuItem *)item;
	mvwprintw(win, y, x, "%.*s", width, mi->label);
}

void draw_checklist_item(WINDOW *win, int y, int x, int width, void *item, bool highlighted)
{
	ChecklistItem *ci = (ChecklistItem *)item;
	char mark = ci->checked ? '+' : ' ';
	mvwprintw(win, y, x, "[%c] %.*s", mark, width - 4, ci->label);
}

void draw_action_menu(WINDOW *win, void *items, int count, int hl, int scroll, int px, int py)
{
	draw_menu_generic(win, items, count, hl, scroll, px, py, sizeof(MenuItem), draw_action_item);
}

void draw_checklist_menu(WINDOW *win, void *items, int count, int hl, int scroll, int px, int py)
{
	draw_menu_generic(win, items, count, hl, scroll, px, py, sizeof(ChecklistItem), draw_checklist_item);
}

void on_action_enter(int idx, void *items)
{
	MenuItem *mi = (MenuItem *)items;
	if (mi[idx].action)
		mi[idx].action();
}

void toggle_checklist_item(int idx, void *items)
{
	ChecklistItem *ci = (ChecklistItem *)items;
	ci[idx].checked = !ci[idx].checked;
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

static WINDOW *g_win_left = NULL;
static WINDOW *g_win_right = NULL;

void menu_loop(Menu *menu)
{
	int hl = 0, ch = ERR, px = 4, py = 2, scroll_offset = 0;
	// WINDOW *win_left = NULL, *win_right = NULL;
	WINDOW *win_left = g_win_left, *win_right = g_win_right;
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

			//////////////////////////////////////////////////////////////////////////////
			g_win_left = win_left;
			g_win_right = win_right;
			//////////////////////////////////////////////////////////////////////////////
		}

		if (COLS < MIN_WIDTH || LINES < MIN_HEIGHT)
		{
			show_resize_warning(COLS, LINES, MIN_WIDTH, MIN_HEIGHT);
			usleep(100000);
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

void run_cmd_in_window(const char *cmd)
{
	WINDOW *win = g_win_right;

	char full_cmd[512];
	snprintf(full_cmd, sizeof(full_cmd), "script -q /dev/null %s", cmd);

	werase(win);
	box(win, 0, 0);
	mvwprintw(win, 0, 2, " Output ");
	wrefresh(win);

	FILE *pipe = popen(full_cmd, "r");
	if (!pipe)
	{
		mvwprintw(win, 1, 1, "Failed to run command");
		wrefresh(win);
		return;
	}

	char buffer[256];
	int row = 1, max_y, max_x;
	getmaxyx(win, max_y, max_x);

	while (fgets(buffer, sizeof(buffer), pipe))
	{
		char clean[256];
		strip_nonprintable(clean, buffer);
		if (row >= max_y - 1)
		{
			wscrl(win, 1);
			row = max_y - 2;
		}
		mvwprintw(win, row++, 1, "%.*s", max_x - 2, clean);
		box(win, 0, 0);
		mvwprintw(win, 0, 2, " Output ");
		wrefresh(win);
	}
	pclose(pipe);
	mvwprintw(win, row++, 1, "--- Process finished ---");
	wrefresh(win);
	wgetch(win);
}

void show_make_menu();
void show_checklist();
void show_main_menu();

int main()
{
	initscr();
	set_escdelay(25);
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);

	show_main_menu();

	endwin();
	return 0;
}

void act_qemu() { run_cmd_in_window("make -C qemu"); }
void act_build() { run_cmd_in_window("make -C ../.. build"); }
void act_run() { run_cmd_in_window("make -C ../.. run"); }
void act_clean() { run_cmd_in_window("make -C ../.. clean"); }
void act_help() { run_cmd_in_window("make -C ../.. help"); }
void act_flash() { run_cmd_in_window("make -C ../.. flash"); }
void act_docker_run() { run_cmd_in_window("make -C ../.. docker-run"); }
void act_docker_build() { run_cmd_in_window("make -C ../.. docker-build"); }
void act_docker_clean() { run_cmd_in_window("make -C ../.. docker-clean"); }

void show_main_menu()
{
	static MenuItem items[] = {
		{"Make", show_make_menu},
		{"List", show_checklist}};

	Menu m = MAKE_MENU(ACTION_MENU, "Main Menu", items);

	menu_loop(&m);
}

void show_make_menu()
{
	static MenuItem items[] = {
		{"host-run", act_qemu},
		{"build", act_build},
		{"run", act_run},
		{"clean", act_clean},
		{"help", act_help},
		{"flash", act_flash},
		{"docker-run", act_docker_run},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-build", act_docker_build},
		{"docker-clean", act_docker_clean}};

	Menu m = MAKE_MENU(ACTION_MENU, "Make Menu", items);
	menu_loop(&m);
}

void show_checklist()
{
	static ChecklistItem items[] = {
		{"item1", true},
		{"item2", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"ite123m3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item333", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item3", false},
		{"item4", false}};

	Menu m = MAKE_MENU(CHECKLIST_MENU, "Checklist", items);
	menu_loop(&m);
}
