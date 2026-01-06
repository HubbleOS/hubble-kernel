#include <stdlib.h>
#include <string.h>
#include <ui/modal.h>
#include <ui/window.h>
#include <ui/main.h>

void show_modal_with_buttons(const char *title, ModalButton *buttons, size_t count)
{
	const int padding = 4;
	const int side_margin = 2;

	int *btn_widths = malloc(count * sizeof(int));
	int total_width = 0;

	for (size_t i = 0; i < count; i++)
	{
		int w = (int)strlen(buttons[i].label) + padding + 2; // +2 for [ ]
		btn_widths[i] = w;
		total_width += w;
	}

	int w = total_width + padding + side_margin * 2;
	int title_width = (int)strlen(title) + 4;
	if (w < title_width)
		w = title_width;
	if (w < 20)
		w = 20;

	const int h = 6;

	int x = (COLS - w) / 2;
	int y = (LINES - h) / 2;

	UIWindow *modal = CREATE_WIN(h, w, y, x, title);

	int gap = (count > 1) ? (w - total_width - side_margin * 2) / (count - 1) : 0;

	int pos_x = (count == 1) ? (w - btn_widths[0]) / 2 : side_margin + 1;
	int btn_y = h / 2 + 1;

	UIElement **btn_elements = malloc(count * sizeof(UIElement *));
	for (size_t i = 0; i < count; i++)
	{
		btn_elements[i] = button_create(buttons[i].label, buttons[i].action, pos_x, btn_y, btn_widths[i], 1);
		ADD_ELEMENT(modal, btn_elements[i]);
		pos_x += btn_widths[i] + gap;
	}

	free(btn_widths);

	int highlighted_idx = 0;
	UIButtonData *data = (UIButtonData *)btn_elements[highlighted_idx]->data;
	data->highlighted = true;

	DRAW_WIN(modal);

	int ch;
	while ((ch = getch()) != 27) // ESC чтобы выйти
	{
		if (ch == KEY_LEFT || ch == KEY_RIGHT)
		{
			((UIButtonData *)btn_elements[highlighted_idx]->data)->highlighted = false;

			if (ch == KEY_LEFT)
				highlighted_idx = (highlighted_idx == 0) ? count - 1 : highlighted_idx - 1;
			else
				highlighted_idx = (highlighted_idx + 1) % count;

			((UIButtonData *)btn_elements[highlighted_idx]->data)->highlighted = true;
			DRAW_WIN(modal);
		}
		else if (ch == '\n' || ch == KEY_ENTER || ch == ' ')
		{
			UIButtonData *btn_data = (UIButtonData *)btn_elements[highlighted_idx]->data;
			if (btn_data->action)
				btn_data->action();
			break;
		}
		else
		{
			WIN_HANDLE_KEY(modal, ch);
			DRAW_WIN(modal);
		}
	}

	free(btn_elements);
	DESTROY_WIN(modal);
}
