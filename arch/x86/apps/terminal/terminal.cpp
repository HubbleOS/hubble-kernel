#include "terminal.h"
#include <utils/font.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Terminal::Terminal(Window &window, uint32_t color)
	: win(window), cursor_X(0), cursor_Y(0), text_color(color),
	  history_count(0), history_index(0), cursor_visible(true),
	  cursor_color(0xFFFFFF)
{
	char_width = CHAR_WIDTH;
	char_height = CHAR_HEIGHT;
	for (size_t i = 0; i < MAX_HISTORY; i++)
		history[i] = nullptr;
}

#include <sys/output_device.h>
#include <sys/input_device.h>
#include <sys/keymap.h>
#include <sys/keyboard.h>

static void terminal_write_adapter(const char *buf, size_t len, void *user_data)
{
	Terminal *term = reinterpret_cast<Terminal *>(user_data);
	for (size_t i = 0; i < len; ++i)
		term->putChar(buf[i]);
}

static size_t terminal_read_adapter(char *buffer, size_t len, void *user_data)
{
	Terminal *term = reinterpret_cast<Terminal *>(user_data);

	size_t i = 0;
	while (i < len)
	{
		key_event_t evt = read_key_event();
		if (evt.released)
			continue;

		char c = keymap_lookup_char(
			evt.id.scancode,
			evt.id.extended,
			evt.is_shift,
			evt.is_caps_lock);

		if (c == 0)
			continue;

		if (evt.id.scancode == KEY_BACKSPACE)
		{
			if (i > 0)
			{
				i--;
				term->putChar('\b');
			}
			continue;
		}

		buffer[i++] = c;
		term->putChar(c);

		if (evt.id.scancode == KEY_ENTER)
			break;
	}

	return i;
}

void Terminal::init()
{
	clear();

	// Output
	static output_device_t terminal_device;
	terminal_device.write = terminal_write_adapter;
	terminal_device.user_data = this;
	register_stdout_device(&terminal_device);

	// Input
	static input_device_t terminal_input;
	terminal_input.read = terminal_read_adapter;
	terminal_input.user_data = this;
	register_stdin_device(&terminal_input);
}

void Terminal::clear()
{
	win.clear();
	cursor_X = 0;
	cursor_Y = 0;
}

void Terminal::scrollUp()
{
	framebuffer_info_t *fb = win.getScreen()->getFramebuffer();
	int win_x = win.getX();
	int win_y = win.getY();
	int win_width = win.getWidth();
	int win_height = win.getHeight();

	// Copy all lines up one line
	for (int y = win_y; y < win_y + win_height - char_height; y++)
	{
		for (int x = win_x; x < win_x + win_width; x++)
		{
			// Copy a pixel from the line below
			uint32_t *pixel_src = (uint32_t *)((uint8_t *)fb->base +
											   (y + char_height) * fb->pitch + x * (fb->bpp / 8));
			uint32_t *pixel_dst = (uint32_t *)((uint8_t *)fb->base +
											   y * fb->pitch + x * (fb->bpp / 8));
			*pixel_dst = *pixel_src;
		}
	}

	// Clearing the last line
	for (int y = win_y + win_height - char_height; y < win_y + win_height; y++)
	{
		for (int x = win_x; x < win_x + win_width; x++)
		{
			uint32_t *pixel = (uint32_t *)((uint8_t *)fb->base +
										   y * fb->pitch + x * (fb->bpp / 8));
			*pixel = win.getBgColor();
		}
	}
}

void Terminal::newLine()
{
	cursor_X = 0;
	cursor_Y += char_height;
	if (cursor_Y + char_height > win.getHeight())
	{
		scrollUp();
		cursor_Y = win.getHeight() - char_height;
	}
}

inline static void drawChar(Window &win, char c, int x, int y, int w, int h, uint32_t color)
{
	framebuffer_info_t *fb = win.getScreen()->getFramebuffer();

	if (c == ' ')
	{
		clear_char_area(fb, win.getX() + x, win.getY() + y, w, h, win.getBgColor());
		return;
	}

	draw_char(fb, c, win.getX() + x, win.getY() + y, w, h, color);
}

void Terminal::putChar(char c)
{
	if (c == '\n')
	{
		newLine();
		return;
	}

	if (c == '\b')
	{
		if (cursor_X >= char_width)
		{
			cursor_X -= char_width;
		}
		else if (cursor_Y >= char_height)
		{
			cursor_Y -= char_height;
			cursor_X = win.getWidth() - char_width;
		}
		else
		{
			return;
		}

		drawChar(win, ' ', cursor_X, cursor_Y, char_width, char_height, text_color);
		return;
	}

	drawChar(win, c, cursor_X, cursor_Y, char_width, char_height, text_color);

	cursor_X += char_width;
	if (cursor_X + char_width > win.getWidth())
		newLine();
}

void Terminal::print(const char *str)
{
	while (*str)
		putChar(*str++);
}

void Terminal::drawCursor()
{
	if (!cursor_visible)
		return;

	drawChar(win, '_', cursor_X, cursor_Y, char_width, char_height, cursor_color);
}

void Terminal::hideCursor()
{
	drawChar(win, ' ', cursor_X, cursor_Y, char_width, char_height, win.getBgColor());
}

void Terminal::addToHistory(const char *line)
{
	if (history_count >= MAX_HISTORY)
	{
		free(history[0]);
		// shift the array to the left
		for (size_t i = 1; i < MAX_HISTORY; i++)
			history[i - 1] = history[i];
		history_count--;
	}

	history[history_count++] = strdup(line);
}

const char *Terminal::getHistory(int &index, int direction)
{
	// direction: -1 = вверх, +1 = вниз
	if (history_count == 0)
		return nullptr;

	index += direction;
	if (index < 0)
		index = 0;
	if (index >= (int)history_count)
	{
		index = history_count;
		return "";
	}

	return history[index];
}

// Checks if the command is valid
static bool isValidCommand(const char *buffer, size_t length)
{
	// List of known commands
	const char *commands[] = {"neofetch", "clear", "help", "cd", "pwd", "echo", "cat"};
	const int num_commands = sizeof(commands) / sizeof(commands[0]);

	// Skip leading spaces
	size_t start = 0;
	while (start < length && (buffer[start] == ' ' || buffer[start] == '\t'))
		start++;

	if (start >= length)
		return false;

	// Finding the first word (command)
	size_t cmd_len = 0;
	while (start + cmd_len < length && buffer[start + cmd_len] != ' ' && buffer[start + cmd_len] != '\n' && buffer[start + cmd_len] != '\t')
		cmd_len++;

	// Compare with famous teams
	for (int i = 0; i < num_commands; i++)
	{
		size_t known_len = strlen(commands[i]);
		if (cmd_len == known_len && strncmp(buffer + start, commands[i], cmd_len) == 0)
			return true;
	}

	return false;
}

char *Terminal::readLine()
{
	size_t capacity = 128;
	size_t length = 0;
	size_t cursor_pos = 0;

	char *buffer = (char *)malloc(capacity);
	if (!buffer)
		return nullptr;

	int line_start_x = cursor_X;
	int line_start_y = cursor_Y;

	size_t prev_length = 0;

	auto clearLine = [&]()
	{
		// Очищаем всю область, где могла быть старая строка
		int x = line_start_x;
		int y = line_start_y;

		for (size_t i = 0; i < prev_length; i++)
		{
			if (y + char_height > win.getHeight())
				break;

			drawChar(win, ' ', x, y, char_width, char_height, win.getBgColor());

			x += char_width;
			if (x + char_width > win.getWidth())
			{
				x = 0;
				y += char_height;
			}
		}
		// Очищаем ещё один символ на всякий случай
		if (y + char_height <= win.getHeight())
			drawChar(win, ' ', x, y, char_width, char_height, win.getBgColor());
	};

	auto redrawLine = [&]()
	{
		// Сначала очищаем старую строку
		clearLine();

		// Обновляем prev_length
		prev_length = length;

		// Определяем цвет для первого слова
		bool is_valid = isValidCommand(buffer, length);
		uint32_t cmd_color = is_valid ? 0x00FF00 : 0xFF0000; // зелёный или красный

		// Пропускаем начальные пробелы для определения конца команды
		size_t start = 0;
		while (start < length && (buffer[start] == ' ' || buffer[start] == '\t'))
			start++;

		// Находим конец первого слова
		size_t first_word_end = start;
		while (first_word_end < length && buffer[first_word_end] != ' ' && buffer[first_word_end] != '\n' && buffer[first_word_end] != '\t')
			first_word_end++;

		int x = line_start_x;
		int y = line_start_y;

		for (size_t i = 0; i < length; i++)
		{
			if (y + char_height > win.getHeight())
				break;

			// Выбираем цвет: команда или обычный текст
			uint32_t color = (i >= start && i < first_word_end) ? cmd_color : text_color;
			drawChar(win, buffer[i], x, y, char_width, char_height, color);

			x += char_width;
			if (x + char_width > win.getWidth())
			{
				x = 0;
				y += char_height;
			}
		}

		// Вычисляем позицию курсора
		cursor_X = line_start_x;
		cursor_Y = line_start_y;
		for (size_t i = 0; i < cursor_pos; i++)
		{
			cursor_X += char_width;
			if (cursor_X + char_width > win.getWidth())
			{
				cursor_X = 0;
				cursor_Y += char_height;
			}
		}
	};

	while (true)
	{
		drawCursor();
		key_event_t evt = read_key_event();
		hideCursor();

		if (evt.released)
			continue;

		// Ctrl + C -> прерывание ввода
		if (evt.is_ctrl && evt.id.scancode == KEY_C)
		{
			// cursor_X = line_start_x;
			// cursor_Y = line_start_y;
			putChar('^');
			putChar('C');
			free(buffer);
			return nullptr;
		}

		// ENTER
		if (evt.id.scancode == KEY_ENTER && !evt.is_shift)
		{
			cursor_X = line_start_x;
			cursor_Y = line_start_y;
			for (size_t i = 0; i < length; i++)
				putChar(buffer[i]);
			putChar('\n');

			buffer[length] = '\0';

			if (length > 0)
				addToHistory(buffer);

			history_index = history_count;
			return buffer;
		}

		// Shift + ENTER
		if (evt.id.scancode == KEY_ENTER && evt.is_shift)
		{
			if (length + 1 >= capacity)
			{
				capacity *= 2;
				char *new_buf = (char *)realloc(buffer, capacity);
				if (!new_buf)
				{
					free(buffer);
					return nullptr;
				}
				buffer = new_buf;
			}

			// Вставляем перенос строки
			for (size_t i = length; i > cursor_pos; i--)
				buffer[i] = buffer[i - 1];
			buffer[cursor_pos] = '\n';
			length++;
			cursor_pos++;

			redrawLine();
			continue;
		}

		// BACKSPACE
		if (evt.id.scancode == KEY_BACKSPACE)
		{
			if (cursor_pos > 0)
			{
				cursor_pos--;
				length--;
				for (size_t i = cursor_pos; i < length; i++)
					buffer[i] = buffer[i + 1];

				redrawLine();
			}
			continue;
		}

		// Стрелка вверх
		if (evt.id.scancode == KEY_UP)
		{
			const char *hist_line = getHistory(history_index, -1);
			if (!hist_line)
				continue;

			// Копируем из истории в буфер
			length = cursor_pos = strlen(hist_line);
			if (length >= capacity)
			{
				capacity = length + 1;
				char *new_buf = (char *)realloc(buffer, capacity);
				if (!new_buf)
				{
					free(buffer);
					return nullptr;
				}
				buffer = new_buf;
			}
			memcpy(buffer, hist_line, length);

			redrawLine();
			continue;
		}

		// Стрелка вниз
		if (evt.id.scancode == KEY_DOWN)
		{
			const char *hist_line = getHistory(history_index, +1);
			if (!hist_line)
				continue;

			// Копируем из истории в буфер
			length = cursor_pos = strlen(hist_line);
			if (length >= capacity)
			{
				capacity = length + 1;
				char *new_buf = (char *)realloc(buffer, capacity);
				if (!new_buf)
				{
					free(buffer);
					return nullptr;
				}
				buffer = new_buf;
			}
			memcpy(buffer, hist_line, length);

			redrawLine();
			continue;
		}

		// Обычные символы
		char c = keymap_lookup_char(evt.id.scancode, evt.id.extended, evt.is_shift, evt.is_caps_lock);
		if (c == 0)
			continue;

		if (length + 1 >= capacity)
		{
			capacity *= 2;
			char *new_buf = (char *)realloc(buffer, capacity);
			if (!new_buf)
			{
				free(buffer);
				return nullptr;
			}
			buffer = new_buf;
		}

		// Вставка символа в позицию курсора
		for (size_t i = length; i > cursor_pos; i--)
			buffer[i] = buffer[i - 1];
		buffer[cursor_pos] = c;
		length++;
		cursor_pos++;

		redrawLine();
	}
}

#include <ctype.h>
static char *trim(char *str)
{
	if (!str)
		return nullptr;

	// Убираем пробелы в начале
	// while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')
	while (isspace(*str))
		str++;

	if (*str == '\0')
		return str;

	// Убираем пробелы в конце
	char *end = str + strlen(str) - 1;
	// while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
	while (end > str && (isspace(*end)))
		end--;

	// Ставим null-terminator после последнего непробельного символа
	*(end + 1) = '\0';

	return str;
}

static int parse_args(char *input, char **argv, int max_args)
{
	int argc = 0;
	char *p = input;
	bool in_quotes = false;
	char quote_char = 0;

	while (*p && argc < max_args)
	{
		while (*p == ' ' || *p == '\t')
			p++;

		if (*p == '\0')
			break;

		if (*p == '"' || *p == '\'')
		{
			in_quotes = true;
			quote_char = *p;
			p++;
			argv[argc++] = p;

			while (*p && *p != quote_char)
				p++;

			if (*p == quote_char)
			{
				*p = '\0';
				p++;
			}
			in_quotes = false;
		}
		else
		{
			argv[argc++] = p;

			while (*p && *p != ' ' && *p != '\t')
				p++;

			if (*p)
			{
				*p = '\0';
				p++;
			}
		}
	}

	return argc;
}

#include <fs/vfs/vfs.h>
#include <fs/vfs/vfs_standart_struct.h>

static void cmd_echo(Terminal *term, int argc, char **argv)
{
	for (int i = 1; i < argc; i++)
	{
		term->print(argv[i]);
		if (i < argc - 1)
			term->print(" ");
	}
	term->print("\n");
}

static void cmd_cat(Terminal *term, int argc, char **argv)
{
	if (argc < 2)
	{
		term->print("Usage: cat <filename>\n");
		return;
	}

	const char *filename = argv[1];

	VFS_File *f = vfs_open(filename, VFS_O_RDONLY);
	if (!f)
	{
		term->print("cat: cannot open '");
		term->print(filename);
		term->print("': No such file or directory\n");
		return;
	}

	char buffer[512];
	size_t bytes_read;

	// while ((bytes_read = vfs_read(f, buffer, sizeof(buffer) - 1)) > 0)
	// {
	// 	buffer[bytes_read] = '\0';
	// 	term->print(buffer);
	// }
	vfs_read(f, buffer, 512);
	printf("%s", buffer);

	term->newLine();

	// vfs_close(f);
}

static void cmd_ls(Terminal *term, int argc, char **argv)
{
	Directory dir = vfs_readdir("/");
	for (size_t i = 0; i < dir.count; i++)
	{
		term->print(dir.entries[i].name);
		term->print("\n");
	}
}

#include "apps/neofetch/neofetch.h"

void Terminal::run()
{
	init();
	while (true)
	{
		print("> ");
		char *input = readLine();
		if (!input)
		{
			print("\n");
			continue;
		}

		char *trimmed = trim(input);

		if (*trimmed == '\0')
		{
			free(input);
			continue;
		}

		char *argv[64];
		int argc = parse_args(trimmed, argv, 64);

		if (argc == 0)
		{
			free(input);
			continue;
		}

		if (strcmp(argv[0], "neofetch") == 0)
		{
			neofetch();
		}
		else if (strcmp(argv[0], "clear") == 0)
		{
			clear();
		}
		else if (strcmp(argv[0], "echo") == 0)
		{
			cmd_echo(this, argc, argv);
		}
		else if (strcmp(argv[0], "cat") == 0)
		{
			cmd_cat(this, argc, argv);
		}
		else if (strcmp(argv[0], "ls") == 0)
		{
			cmd_ls(this, argc, argv);
		}
		else if (strcmp(argv[0], "help") == 0)
		{
			print("Available commands:\n");
			print("  neofetch - Show system information\n");
			print("  clear    - Clear the terminal\n");
			print("  echo     - Display a line of text\n");
			print("  cat      - Display file contents\n");
			print("  help     - Show this help message\n");
			print("  exit     - Exit the terminal\n");
		}
		else
		{
			print("Unknown command: ");
			print(argv[0]);
			print("\n");
		}

		free(input);
	}
}
