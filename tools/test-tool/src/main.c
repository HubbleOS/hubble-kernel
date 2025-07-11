#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct
{
	const char *label;
	void (*action)(void);
} MenuItem;

typedef struct
{
	const char *label;
	int checked;
} ChecklistItem;

#define sizeof_array(a) (sizeof(a) / sizeof(a[0]))

void menu_navigation(MenuItem items[], size_t n_items);

void run_command(const char *cmd)
{
	endwin();
	system(cmd);
	initscr();
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);
}

void action_make_host_run() { run_command("make -C ../.. host-run"); }
void action_make_build() { run_command("make -C ../.. build"); }
void action_make_run() { run_command("make -C ../.. run"); }
void action_make_clean() { run_command("make -C ../.. clean"); }
void action_make_help() { run_command("make -C ../.. help"); }
void action_make_flash() { run_command("make -C ../.. flash"); }
void action_make_docker_run() { run_command("make -C ../.. docker-run"); }
void action_make_docker_build() { run_command("make -C ../.. docker-build"); }
void action_make_docker_clean() { run_command("make -C ../.. docker-clean"); }

void show_make_menu()
{
	MenuItem make_menu[] = {
		{"host-run", action_make_host_run},
		{"build", action_make_build},
		{"run", action_make_run},
		{"clean", action_make_clean},
		{"help", action_make_help},
		{"flash", action_make_flash},
		{"docker-run", action_make_docker_run},
		{"docker-build", action_make_docker_build},
		{"docker-clean", action_make_docker_clean},
		{"< Back", NULL}};

	menu_navigation(make_menu, sizeof_array(make_menu));
}

void show_checklist_menu()
{
	ChecklistItem checklist[] = {
		{"item1", true},
		{"item2", false},
		{"item3", false},
		{"item4", false},
	};

	size_t n_items = sizeof_array(checklist);
	int highlight = 0;
	int ch;

	int padding_y = 2;
	int padding_x = 4;
	int max_label_len = 0;

	for (size_t i = 0; i < n_items; i++)
	{
		int len = (int)strlen(checklist[i].label);
		if (len > max_label_len)
			max_label_len = len;
	}

	int win_height = (int)n_items + padding_y * 2;
	int win_width = max_label_len + padding_x * 2 + 6;
	int starty = (LINES - win_height) / 2;
	int startx = (COLS - win_width) / 2;

	WINDOW *menu_win = newwin(win_height, win_width, starty, startx);
	keypad(menu_win, TRUE);

	while (1)
	{
		werase(menu_win);
		box(menu_win, 0, 0);

		for (size_t i = 0; i < n_items; i++)
		{
			char mark = checklist[i].checked ? '+' : ' ';
			int y = (int)i + padding_y;
			int x = padding_x;

			if ((int)i == highlight)
			{
				wattron(menu_win, A_REVERSE);
				mvwprintw(menu_win, y, x, "[%c] %s", mark, checklist[i].label);
				wattroff(menu_win, A_REVERSE);
			}
			else
			{
				mvwprintw(menu_win, y, x, "[%c] %s", mark, checklist[i].label);
			}
		}
		wrefresh(menu_win);

		ch = wgetch(menu_win);
		if (ch == KEY_UP)
		{
			highlight = (highlight - 1 + n_items) % n_items;
		}
		else if (ch == KEY_DOWN)
		{
			highlight = (highlight + 1) % n_items;
		}
		else if (ch == '\n' || ch == ' ')
		{
			checklist[highlight].checked = !checklist[highlight].checked;
		}
		else if (ch == 27)
		{
			break;
		}
	}
	delwin(menu_win);
}

void menu_navigation(MenuItem items[], size_t n_items)
{
	int highlight = 0;
	int ch;

	int padding_y = 2;
	int padding_x = 4;
	int max_label_len = 0;

	for (size_t i = 0; i < n_items; i++)
	{
		int len = (int)strlen(items[i].label);
		if (len > max_label_len)
			max_label_len = len;
	}

	int win_height = (int)n_items + padding_y * 2;
	int win_width = max_label_len + padding_x * 2;

	int starty = (LINES - win_height) / 2;
	int startx = (COLS - win_width) / 2;

	WINDOW *menu_win = newwin(win_height, win_width, starty, startx);
	box(menu_win, 0, 0);
	keypad(menu_win, TRUE);

	while (1)
	{

		werase(menu_win);
		box(menu_win, 0, 0);

		for (size_t i = 0; i < n_items; i++)
		{
			int y = (int)i + padding_y;
			int x = padding_x;

			if ((int)i == highlight)
			{
				wattron(menu_win, A_REVERSE);
				mvwprintw(menu_win, y, x, "%s", items[i].label);
				wattroff(menu_win, A_REVERSE);
			}
			else
			{
				mvwprintw(menu_win, y, x, "%s", items[i].label);
			}
		}
		wrefresh(menu_win);

		ch = wgetch(menu_win);
		if (ch == KEY_UP)
		{
			highlight = (highlight - 1 + n_items) % n_items;
		}
		else if (ch == KEY_DOWN)
		{
			highlight = (highlight + 1) % n_items;
		}
		else if (ch == '\n')
		{
			delwin(menu_win);
			clear();
			refresh();

			if (items[highlight].action)
			{
				items[highlight].action();
			}
			else
			{
				break;
			}

			menu_win = newwin(win_height, win_width, starty, startx);
			keypad(menu_win, TRUE);
		}
	}
	delwin(menu_win);
}

int main()
{
	MenuItem main_menu[] = {
		{"make", show_make_menu},
		{"list", show_checklist_menu},
		{"Exit", NULL}};

	initscr();
	noecho();
	curs_set(FALSE);
	keypad(stdscr, TRUE);

	menu_navigation(main_menu, sizeof_array(main_menu));

	endwin();
	return 0;
}
