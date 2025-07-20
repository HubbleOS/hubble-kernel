#include <ui/windows.h>
#include <ui/text.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	char *text;
} UITextData;

static void text_draw(UIElement *elem, WINDOW *win)
{
	UITextData *data = (UITextData *)elem->data;

	if (data && data->text)
	{
		mvwprintw(win, elem->y, elem->x, "%s", data->text);
	}
}

static bool text_handle_key(UIElement *elem, int ch)
{
	// Текстовый элемент не обрабатывает ввод
	return false;
}

UIElement *text_create(const char *text, int x, int y)
{
	UIElement *elem = malloc(sizeof(UIElement));
	UITextData *data = malloc(sizeof(UITextData));
	data->text = strdup(text);

	elem->data = data;
	elem->x = x;
	elem->y = y;
	elem->w = strlen(text);
	elem->h = 1;
	elem->draw = text_draw;
	elem->handle_key = text_handle_key;
	elem->parent = NULL;

	return elem;
}

void text_set(UIElement *elem, const char *new_text)
{
	UITextData *data = (UITextData *)elem->data;
	free(data->text);
	data->text = strdup(new_text);
}
