#pragma once

#include <stddef.h>

#include "../window/window.h"
#include "utils/font.h"

#define MAX_HISTORY 64

class Terminal
{
private:
	Window &win;

	int cursor_X;
	int cursor_Y;
	bool cursor_visible = true;
	uint32_t cursor_color = 0xFFFFFF;

	int char_width;
	int char_height;
	uint32_t text_color;

	char *history[MAX_HISTORY] = {nullptr};
	size_t history_count = 0; // сколько команд всего
	int history_index = -1;	  // текущая позиция при навигации по истории

public:
	Terminal(Window &window, uint32_t text_color = 0xFFFFFF);
	void init();
	void clear();
	void putChar(char c);
	void print(const char *str);

	char *readLine();
	void newLine();

	void addToHistory(const char *str);
	const char *getHistory(int &index, int direction);

	void drawCursor();
	void hideCursor();

	void scrollUp();

	void run();
};
