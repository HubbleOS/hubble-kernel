#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>

#include "menutypes.h"

typedef enum
{
	MENU_ACTION,
	MENU_CHECKLIST
} MenuType;

typedef struct
{
	MenuType type;
	const char *title;
	union
	{
		struct
		{
			MenuItem *items;
			size_t count;
		} action;
		struct
		{
			ChecklistItem *items;
			size_t count;
		} checklist;
	};
} Menu;

#define sizeOfArray(arr) (sizeof(arr) / sizeof((arr)[0]))
#define COUNT(arr) sizeOfArray(arr)

volatile sig_atomic_t resized = 0;

void on_resize(int sig)
{
	(void)sig;
	resized = 1;
}

void strip_nonprintable(char *dest, const char *src)
{
	const char *s = src;
	char *d = dest;

	while (*s)
	{
		if ((*s >= 32 && *s <= 126) || *s == '\n' || *s == '\t')
			*d++ = *s;
		s++;
	}
	*d = '\0';
}

void run_cmd_in_window(const char *cmd, WINDOW *win)
{
	char full_cmd[512];
	snprintf(full_cmd, sizeof(full_cmd), "script -q /dev/null %s", cmd);
	// snprintf(full_cmd, sizeof(full_cmd), "%s", cmd);

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
		char clean_buf[256];
		strip_nonprintable(clean_buf, buffer);

		if (row >= max_y - 1)
		{
			wscrl(win, 1);
			row = max_y - 2;
		}
		mvwprintw(win, row++, 1, "%.*s", max_x - 2, clean_buf);
		wrefresh(win);

		box(win, 0, 0);
		mvwprintw(win, 0, 2, " Output ");
		wrefresh(win);
	}
	pclose(pipe);

	mvwprintw(win, row++, 1, "--- Process finished ---");
	wrefresh(win);

	wgetch(win);
}

void draw_action_menu(WINDOW *win, MenuItem *items, int count, int hl, int scroll, int px, int py)
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
		mvwprintw(win, y, px, "%s", items[idx].label);
		if (idx == hl)
			wattroff(win, A_REVERSE);
	}
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

void draw_frame(WINDOW *win, const char *title)
{
	box(win, 0, 0);
	if (title)
	{
		int x = (getmaxx(win) - (int)strlen(title)) / 2;
		mvwprintw(win, 0, x > 1 ? x : 1, " %s ", title);
	}
}

void draw_checklist_menu(WINDOW *win, ChecklistItem *items, int count, int hl, int scroll, int px, int py)
{
	int height = getmaxy(win) - py - 2;
	int width = getmaxx(win) - px - 4;
	for (int i = 0; i < height && (i + scroll) < count; i++)
	{
		int idx = i + scroll;
		int y = i + py;
		char mark = items[idx].checked ? '+' : ' ';
		mvwhline(win, y, px, ' ', width);
		if (idx == hl)
		{
			wattron(win, A_REVERSE);
			mvwprintw(win, y, px, "[%c] %s", mark, items[idx].label);
			wattroff(win, A_REVERSE);
		}
		else
		{
			mvwprintw(win, y, px, "[%c] %s", mark, items[idx].label);
		}
	}
}

#include <sys/select.h>
#include <unistd.h>

void menu_loop(Menu *menu)
{
	int hl = 0, ch = ERR, px = 4, py = 2, scroll_offset = 0;
	WINDOW *win_left = NULL, *win_right = NULL;
	const int MIN_WIDTH = 80, MIN_HEIGHT = 24;
	struct sigaction sa = {.sa_handler = on_resize, .sa_flags = SA_RESTART};
	sigaction(SIGWINCH, &sa, NULL);

	int term_width, term_height;

	while (1)
	{
		if (resized)
		{
			resized = 0;
			endwin();
			refresh();
			clear();
			if (win_left)
				delwin(win_left);
			if (win_right)
				delwin(win_right);
			win_left = NULL;
			win_right = NULL;
		}

		term_width = COLS;
		term_height = LINES;

		if (term_width < MIN_WIDTH || term_height < MIN_HEIGHT)
		{
			show_resize_warning(term_width, term_height, MIN_WIDTH, MIN_HEIGHT);
			usleep(100000);
			continue;
		}

		if (!win_left || !win_right)
		{
			int left_width = term_width * 0.35;
			int right_width = term_width - left_width;
			win_left = newwin(term_height, left_width, 0, 0);
			win_right = newwin(term_height, right_width, 0, left_width);
			keypad(win_left, TRUE);
			nodelay(win_left, TRUE); // Non-blocking input mode
		}

		werase(win_left);
		draw_frame(win_left, menu->title);
		int count = 0;
		if (menu->type == MENU_ACTION)
		{
			count = (int)menu->action.count;
			int max_visible = getmaxy(win_left) - py - 2;
			if (hl < scroll_offset)
				scroll_offset = hl;
			if (hl >= scroll_offset + max_visible)
				scroll_offset = hl - max_visible + 1;
			draw_action_menu(win_left, menu->action.items, count, hl, scroll_offset, px, py);
		}
		else if (menu->type == MENU_CHECKLIST)
		{
			count = (int)menu->checklist.count;
			int max_visible = getmaxy(win_left) - py - 2;
			if (hl < scroll_offset)
				scroll_offset = hl;
			if (hl >= scroll_offset + max_visible)
				scroll_offset = hl - max_visible + 1;
			draw_checklist_menu(win_left, menu->checklist.items, count, hl, scroll_offset, px, py);
		}

		werase(win_right);
		draw_frame(win_right, "Output");
		wrefresh(win_right);
		wrefresh(win_left);

		// Wait for input or timeout using select
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(0, &readfds); // stdin = 0

		struct timeval tv = {0, 100000}; // 100 ms

		int ret = select(1, &readfds, NULL, NULL, &tv);
		if (ret > 0 && FD_ISSET(0, &readfds))
		{
			ch = wgetch(win_left);
		}
		else
		{
			ch = ERR;
		}

		if (ch == KEY_UP)
			hl = (hl - 1 + count) % count;
		else if (ch == KEY_DOWN)
			hl = (hl + 1) % count;
		else if (ch == '\n')
		{
			if (hl < count)
			{
				switch (menu->type)
				{
				case MENU_ACTION:
					if (menu->action.items[hl].action)
					{
						menu->action.items[hl].action(win_right);
					}
					break;
				case MENU_CHECKLIST:
					menu->checklist.items[hl].checked = !menu->checklist.items[hl].checked;
				default:
					break;
				}
			}
		}
		else if (ch == 27) // ESC
			break;
	}

	if (win_left)
		delwin(win_left);
	if (win_right)
		delwin(win_right);
	clear();
	refresh();
}

void show_make_menu(WINDOW *output_win);
void show_checklist(WINDOW *output_win);
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

void act_qemu(WINDOW *win) { run_cmd_in_window("make -C qemu", win); }
void act_build(WINDOW *win) { run_cmd_in_window("make -C ../.. build", win); }
void act_run(WINDOW *win) { run_cmd_in_window("make -C ../.. run", win); }
void act_clean(WINDOW *win) { run_cmd_in_window("make -C ../.. clean", win); }
void act_help(WINDOW *win) { run_cmd_in_window("make -C ../.. help", win); }
void act_flash(WINDOW *win) { run_cmd_in_window("make -C ../.. flash", win); }
void act_docker_run(WINDOW *win) { run_cmd_in_window("make -C ../.. docker-run", win); }
void act_docker_build(WINDOW *win) { run_cmd_in_window("make -C ../.. docker-build", win); }
void act_docker_clean(WINDOW *win) { run_cmd_in_window("make -C ../.. docker-clean", win); }

void show_main_menu()
{
	static MenuItem items[] = {
		{"Make", show_make_menu},
		{"List", show_checklist}};

	Menu m = {.type = MENU_ACTION, .title = "Main Menu", .action = {items, COUNT(items)}};

	menu_loop(&m);
}

void show_make_menu(WINDOW *output_win)
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
	Menu m = {.type = MENU_ACTION, .title = "Make Menu", .action = {items, COUNT(items)}};
	menu_loop(&m);
}

void show_checklist(WINDOW *output_win)
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
	Menu m = {.type = MENU_CHECKLIST, .title = "Checklist", .checklist = {items, COUNT(items)}};
	menu_loop(&m);
}
