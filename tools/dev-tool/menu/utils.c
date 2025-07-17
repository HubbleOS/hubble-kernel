#include <ncurses.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "utils.h"

extern WINDOW *g_win_left, *g_win_right;

static void strip_nonprintable(char *dest, const char *src)
{
	while (*src)
	{
		if (isprint(*src) || *src == '\n' || *src == '\t')
			*dest++ = *src;
		src++;
	}
	*dest = '\0';
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

	char buffer[256], clean[256];
	int row = 1, max_y, max_x;
	getmaxyx(win, max_y, max_x);

	while (fgets(buffer, sizeof(buffer), pipe))
	{
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
