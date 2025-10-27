#include "app.h"

#define MIN_WIDTH 80
#define MIN_HEIGHT 24

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
