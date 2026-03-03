#pragma once

#include "window.h"
#include <stdint.h>
#include <stddef.h>

typedef struct
{
	window_t *win; // окно для консоли
	int cursor_x, cursor_y;
	color_t font_color;
	color_t bg_color;
} console_t;

// Создание консоли в окне
console_t *console_create(int x, int y, int w, int h, color_t font, color_t bg);

// Вывод символа
void console_putc(console_t *con, char c);

// Вывод строки
void console_write(console_t *con, const char *str, size_t len);

// Очистка консоли
void console_clear(console_t *con);

// Отрисовка на экран
void console_render(console_t *con);
