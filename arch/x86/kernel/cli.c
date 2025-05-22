#include "cli.h"
#include "stdbool.h"
#include <stddef.h>
#include "utils/framebuffer.h"
#include "utils/font.h"
#include "utils/color.h"

int *bufer_history;
char *key_history[50];
uint8_t key_history_it[128];

uint16_t t_colums = 80;
uint16_t t_rows = 25;
uint8_t t_row = 0;
uint8_t t_col = 0;
uint8_t t_color = 0x0f;
uint32_t t_color_char = 0xFFFFFF;

uint16_t t_pos_x = 200;
uint16_t t_pos_y = 200;

uint16_t cursor_x = 0;
uint16_t cursor_y = 0;
uint16_t cursor_visible = 1;
uint16_t t_width = 0;
uint16_t t_height = 0;
framebuffer_info_t *fbcli = 0;

#define GET_MACRO(_1, _2, NAME, ...) NAME
#define print(...) GET_MACRO(__VA_ARGS__, print_colored, print)(__VA_ARGS__)

void print_char(char c);
void print(const char *str);
void print_colored(const char *str, color color);
int strcmp(const char *s1, const char *s2);
void print_hex(uint8_t value);
uint8_t strlen(const char *str);

void handle_key_up(char *buf, int *i, int *buf_it);
void handle_key_down(char *buf, int *i, int *buf_it);
void handle_backspace(int *i);
void handle_regular_char(char c, char *buf, int *i);

void wait(volatile unsigned int ticks)
{
	for (volatile unsigned int i = 0; i < ticks; i++)
		;
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t ret;
	__asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

static inline void outb(uint16_t port, uint8_t val)
{
	__asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

char scancode_to_ascii[128] = {
	0, 27, '1', '2', '3', '4', '5', '6', '7', '8', // 0x00 - 0x09
	'9', '0', '-', '=', '\b', '\t',				   // ...
	'q', 'w', 'e', 'r',							   // ...
	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',  // ...
	0,											   // Ctrl
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
	0, // Left shift
	'\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
	0,			 // Right shift
	'*', 0, ' ', // Spacebar
				 // ... (можна розширити за потребою)
};

#define KEY_UP 0x80
#define KEY_DOWN 0x81
#define KEY_LEFT 0x82
#define KEY_RIGHT 0x83

int read_ps2_key()
{
	uint8_t scancode = 0;
	bool is_extended = false;

	while (1)
	{

		// Чекаємо, поки з'являться дані
		while (!(inb(0x64) & 1))
			;

		scancode = inb(0x60);

		if (scancode == 0xE0)
		{
			is_extended = true;
			while (!(inb(0x64) & 1))
				;
			scancode = inb(0x60);
		}

		if (scancode & 0x80)
			continue;

		break;
	}

	if (is_extended)
	{
		switch (scancode)
		{
		case 0x48:
			return KEY_UP;
		case 0x50:
			return KEY_DOWN;
		case 0x4B:
			return KEY_LEFT;
		case 0x4D:
			return KEY_RIGHT;
		default:
			return 0;
		}
	}

	return scancode_to_ascii[scancode];
}

void terminal_putentryat(char c) { draw_char(fbcli, c, t_pos_x + (t_col * 8), t_pos_y + (t_row * 8), t_color_char); }
// void move_cursor(uint8_t row, uint8_t col) { return; }

void clear_line(int len)
{
	for (int j = 0; j < len; j++)
		print_char(' ');
}

void handle_key_up(char *buf, int *i, int *buf_it)
{
	if (*buf_it == 0)
		return;

	(*buf_it)--;
	t_col = 2;

	clear_line(strlen(buf));

	int len = strlen(key_history[*buf_it]);
	for (int j = 0; j < len; j++)
		buf[j] = key_history[*buf_it][j];
	buf[len] = '\0';
	*i = len;

	t_col = 2;
	print(buf);
}

void handle_key_down(char *buf, int *i, int *buf_it)
{
	if (*buf_it >= bufer_history - 1)
		return;

	(*buf_it)++;
	t_col = 2;

	clear_line(strlen(buf));

	int len = strlen(key_history[*buf_it]);
	for (int j = 0; j < len; j++)
		buf[j] = key_history[*buf_it][j];
	buf[len] = '\0';
	*i = len;

	t_col = 2;
	print(buf);
}

void handle_backspace(int *i)
{
	if (*i > 0 && t_col > 0)
	{
		(*i)--;
		t_col--;
		terminal_putentryat(' ');
		// move_cursor(t_row, t_col);
	}
}

void handle_regular_char(char c, char *buf, int *i)
{

	buf[(*i)++] = c;

	print_char(c);
}

void read_line(char *buf, int max_len)
{
	int buf_it = *bufer_history;
	int i = 0;
	while (i < max_len - 1)
	{
		int c = read_ps2_key();
		if ((uint8_t)c == KEY_UP)
		{
			handle_key_up(buf, &i, &buf_it);
			continue;
		}
		if ((uint8_t)c == KEY_DOWN)
		{
			handle_key_down(buf, &i, &buf_it);
			continue;
		}
		if (c == '\b')
		{
			handle_backspace(&i);
			continue;
		}

		if (c == '\n')
		{
			print("\n");
			break;
		}

		handle_regular_char(c, buf, &i);
	}
	buf[i] = '\0';
}

int atoi(const char *str)
{
	int result = 0;
	while (*str >= '0' && *str <= '9')
	{
		result = result * 10 + (*str - '0');
		str++;
	}
	return result;
}

bool is_blank(const char *str)
{
	while (*str)
	{
		if (*str != ' ' && *str != '\t')
			return false;
		str++;
	}
	return true;
}

typedef enum
{
	CMD_UNKNOWN,
	CMD_EXIT,
	CMD_HELLO,
	CMD_NEOFETCH,
	CMD_CLEAR,
	CMD_CHANGE_X,
	CMD_CHANGE_Y,
	CMD_HELP
} Command;

Command command_from_string(const char *input)
{

	if (strcmp(input, "exit\0") == 0)
		return CMD_EXIT;
	if (strcmp(input, "hello") == 0)
		return CMD_HELLO;
	if (strcmp(input, "neofetch") == 0)
		return CMD_NEOFETCH;
	if (strcmp(input, "clear") == 0)
		return CMD_CLEAR;
	if (strcmp(input, "change-x") == 0)
		return CMD_CHANGE_X;
	if (strcmp(input, "change-y") == 0)
		return CMD_CHANGE_Y;
	if (strcmp(input, "help") == 0)
		return CMD_HELP;
	return CMD_UNKNOWN;
}
void handle_exit() { print("\nGoodbye!\n"); }
void handle_hello() { print("\nHello!\n"); }

void handle_clear()
{
	uint32_t *pixels = (uint32_t *)fbcli->base;
	for (size_t i = 0; i < (fbcli->pitch / 4) * fbcli->height; ++i)
		pixels[i] = 0x000000;

	t_col = t_row = 0;
}

void handle_neofetch()
{
	print(
		" _    _         _      _      _        \n"
		"| |  | |       | |    | |    | |       \n"
		"| |__| | _   _ | |__  | |__  | |  ___  \n"
		"|  __  || | | || '_ \\ | '_ \\ | | / _ \\ \n"
		"| |  | || |_| || |_) || |_) || ||  __/ \n"
		"|_|  |_| \\__,_||_.__/ |_.__/ |_| \\___| \n",
		rgb(255, 150, 80));
}

void handle_help()
{
	print("\nCommands:\n");
	print("  exit\n");
	print("  hello\n");
	print("  neofetch\n");
	print("  clear\n");
	print("  help\n");
	print("  change-x\n");
	print("  change-y\n");
}

void handle_change_x()
{
	char input[100];
	print("Enter new X: ");
	read_line(input, 100);
	t_pos_x = atoi(input);
}

void handle_change_y()
{
	char input[100];
	print("Enter new Y: ");
	read_line(input, 100);
	t_pos_y = atoi(input);
}

typedef int (*app_entry_t)(void);

void run_app(void *app_binary)
{
	app_entry_t app_main = (app_entry_t)app_binary;
	app_main();
}

int cli(framebuffer_info_t *fb)
{
	fbcli = fb;
	uint32_t *pixels = (uint32_t *)fb->base;

	handle_clear();
	handle_neofetch();

	for (int i = 0; i < 50; i++)
	{
		key_history[i] = kmalloc(100);
		key_history[i][0] = '\0';
		if (key_history[i] == NULL)
		{
			print("Out of memory!\n");
			return 0;
		}
	}

	bufer_history = kmalloc(sizeof(int));
	if (bufer_history != NULL)
	{
		*bufer_history = 0;
	}
	while (1)
	{
		t_col = 0;
		print("> ", COLOR_GREEN);
		read_line(key_history[*bufer_history], 100);
		const char *input = key_history[*bufer_history];
		if (input[0] == '\0' || is_blank(input))
		{
			print("\n");
			continue;
		}

		++*bufer_history;

		Command cmd = command_from_string(input);

		switch (cmd)
		{
		case CMD_EXIT:
			handle_exit();
			return 0;
		case CMD_HELLO:
			handle_hello();
			break;
		case CMD_CLEAR:
			handle_clear();
			break;
		case CMD_NEOFETCH:
			handle_neofetch();
			break;
		case CMD_HELP:
			handle_help();
			break;
		case CMD_CHANGE_X:
			handle_change_x();
			break;
		case CMD_CHANGE_Y:
			handle_change_y();
			break;

		default:
			print("\nUnknown command!\n");
			break;
		}
	}
}

void print_hex(uint8_t value)
{
	char hex_digits[] = "0123456789ABCDEF";
	char buf[5];

	buf[0] = '0';
	buf[1] = 'x';
	buf[2] = hex_digits[(value >> 4) & 0x0F];
	buf[3] = hex_digits[value & 0x0F];
	buf[4] = '\0';

	print(buf);
}
void print_char(char c)
{
	if (c == '\n' || t_col >= t_colums)
	{
		t_row++;
		t_col = 0;
		if (c == '\n')
			return;
	}
	terminal_putentryat(c);
	t_col++;
}

void print(const char *str)
{
	while (*str)
		print_char(*str++);
}

void print_colored(const char *str, uint32_t color)
{
	t_color_char = color;
	print(str);
	t_color_char = COLOR_WHITE;
}

int strcmp(const char *s1, const char *s2)
{
	while (*s1 && (*s1 == *s2))
	{

		s1++;
		s2++;
	}
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

uint8_t strlen(const char *str)
{
	uint8_t len = 0;
	while (str[len])
		len++;
	return len;
}
