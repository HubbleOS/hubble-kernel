#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>

typedef enum
{
	MENU_ACTION,
	MENU_CHECKLIST
} MenuType;

typedef struct
{
	const char *label;
	void (*action)(WINDOW *output_win);
} MenuItem;

typedef struct
{
	const char *label;
	bool checked;
} ChecklistItem;

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

#define COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

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
	}
	pclose(pipe);

	mvwprintw(win, row++, 1, "--- Process finished ---");
	wrefresh(win);

	wgetch(win);
}

void act_host_run(WINDOW *win) { run_cmd_in_window("make -C ../.. host-run", win); }
void act_build(WINDOW *win) { run_cmd_in_window("make -C ../.. build", win); }
void act_run(WINDOW *win) { run_cmd_in_window("make -C ../.. run", win); }
void act_clean(WINDOW *win) { run_cmd_in_window("make -C ../.. clean", win); }
void act_help(WINDOW *win) { run_cmd_in_window("make -C ../.. help", win); }
void act_flash(WINDOW *win) { run_cmd_in_window("make -C ../.. flash", win); }
void act_docker_run(WINDOW *win) { run_cmd_in_window("make -C ../.. docker-run", win); }
void act_docker_build(WINDOW *win) { run_cmd_in_window("make -C ../.. docker-build", win); }
void act_docker_clean(WINDOW *win) { run_cmd_in_window("make -C ../.. docker-clean", win); }

void draw_frame(WINDOW *win, const char *title)
{
	box(win, 0, 0);
	if (title)
	{
		int x = (getmaxx(win) - (int)strlen(title)) / 2;
		mvwprintw(win, 0, x > 1 ? x : 1, " %s ", title);
	}
}

void draw_action_menu(WINDOW *win, MenuItem *items, int count, int hl, int px, int py)
{
	int width = getmaxx(win) - px - 4;
	for (int i = 0; i < count; i++)
	{
		int y = i + py;
		if (i == hl)
		{
			wattron(win, A_REVERSE);
			mvwhline(win, y, px, ' ', width);
			mvwprintw(win, y, px, "%s", items[i].label);
			wattroff(win, A_REVERSE);
		}
		else
		{
			mvwprintw(win, y, px, "%s", items[i].label);
		}
	}
	int y = count + py;
	if (hl == count)
	{
		wattron(win, A_REVERSE);
		mvwhline(win, y, px, ' ', width);
		mvwprintw(win, y, px, "< Back");
		wattroff(win, A_REVERSE);
	}
	else
	{
		mvwprintw(win, y, px, "< Back");
	}
}

void draw_checklist_menu(WINDOW *win, ChecklistItem *items, int count, int hl, int px, int py)
{
	int width = getmaxx(win) - px - 4;
	for (int i = 0; i < count; i++)
	{
		int y = i + py;
		char mark = items[i].checked ? '+' : ' ';
		if (i == hl)
		{
			wattron(win, A_REVERSE);
			mvwhline(win, y, px, ' ', width);
			mvwprintw(win, y, px, "[%c] %s", mark, items[i].label);
			wattroff(win, A_REVERSE);
		}
		else
		{
			mvwprintw(win, y, px, "[%c] %s", mark, items[i].label);
		}
	}
	int y = count + py;
	if (hl == count)
	{
		wattron(win, A_REVERSE);
		mvwhline(win, y, px, ' ', width);
		mvwprintw(win, y, px, "< Back");
		wattroff(win, A_REVERSE);
	}
	else
	{
		mvwprintw(win, y, px, "< Back");
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
	wgetch(win);
	delwin(win);
}

void menu_loop(Menu *menu)
{
	int hl = 0, ch, px = 4, py = 2;
	WINDOW *win_left = NULL;
	WINDOW *win_right = NULL;
	const int min_h = 24;

	struct sigaction sa = {.sa_handler = on_resize, .sa_flags = SA_RESTART};
	sigaction(SIGWINCH, &sa, NULL);

	while (1)
	{
		if (resized)
		{
			resized = 0;
			endwin();
			refresh();
			clear();
		}

		if (win_left)
		{
			delwin(win_left);
			win_left = NULL;
		}
		if (win_right)
		{
			delwin(win_right);
			win_right = NULL;
		}

		int termw = COLS, termh = LINES;
		int w = termw < 80 ? termw : 80;
		int h = min_h;

		if (h > termh || w > termw)
		{
			show_resize_warning(termw, termh, w, h);
			timeout(100);
			ch = getch();
			if (ch == 27)
				break;
			continue;
		}

		int left_w = w / 2;
		int right_w = w - left_w;
		int x = (termw - w) / 2;
		int y = (termh - h) / 2;

		win_left = newwin(h, left_w, y, x);
		win_right = newwin(h, right_w, y, x + left_w);

		keypad(win_left, TRUE);

		werase(win_left);
		draw_frame(win_left, menu->title);

		int count = 0;
		if (menu->type == MENU_ACTION)
		{
			count = (int)menu->action.count;
			draw_action_menu(win_left, menu->action.items, count, hl, px, py);
		}
		else if (menu->type == MENU_CHECKLIST)
		{
			count = (int)menu->checklist.count;
			draw_checklist_menu(win_left, menu->checklist.items, count, hl, px, py);
		}

		werase(win_right);
		draw_frame(win_right, "Output");
		wrefresh(win_right);

		wrefresh(win_left);

		timeout(-1);
		ch = wgetch(win_left);

		if (ch == KEY_UP)
			hl = (hl - 1 + count + 1) % (count + 1);
		else if (ch == KEY_DOWN)
			hl = (hl + 1) % (count + 1);
		else if (ch == '\n')
		{
			if (hl < count)
			{
				if (menu->type == MENU_ACTION && menu->action.items[hl].action)
				{

					menu->action.items[hl].action(win_right);

					werase(win_left);
					draw_frame(win_left, menu->title);
					draw_action_menu(win_left, menu->action.items, count, hl, px, py);
					wrefresh(win_left);

					werase(win_right);
					draw_frame(win_right, "Output");
					wrefresh(win_right);
				}
				else if (menu->type == MENU_CHECKLIST)
				{
					menu->checklist.items[hl].checked ^= 1;
				}
			}
			else
				break;
		}
		else if (ch == 27)
			break;
	}

	if (win_left)
		delwin(win_left);
	if (win_right)
		delwin(win_right);
	clear();
	refresh();
}

void show_make_menu(WINDOW *output_win)
{
	static MenuItem items[] = {
		{"host-run", act_host_run},
		{"build", act_build},
		{"run", act_run},
		{"clean", act_clean},
		{"help", act_help},
		{"flash", act_flash},
		{"docker-run", act_docker_run},
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
		{"item4", false}};
	Menu m = {.type = MENU_CHECKLIST, .title = "Checklist", .checklist = {items, COUNT(items)}};
	menu_loop(&m);
}

int main()
{
	initscr();
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);

	MenuItem main_items[] = {
		{"Make", show_make_menu},
		{"List", show_checklist}};
	Menu main_menu = {
		.type = MENU_ACTION,
		.title = "Main Menu",
		.action = {main_items, COUNT(main_items)}};

	menu_loop(&main_menu);

	endwin();
	return 0;
}
