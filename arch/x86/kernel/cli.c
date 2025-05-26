// #include "cli.h"
// #include "stdbool.h"
// #include <stddef.h>
// #include "utils/framebuffer.h"
// #include "utils/font.h"
// #include "utils/color.h"

// int *bufer_history;
// char *key_history[50];
// uint8_t key_history_it[128];

// uint16_t t_colums = 80;
// uint16_t t_rows = 25;
// uint8_t t_row = 0;
// uint8_t t_col = 0;
// uint8_t t_color = 0x0f;
// uint32_t t_color_char = 0xFFFFFF;

// uint16_t t_pos_x = 200;
// uint16_t t_pos_y = 200;

// uint16_t cursor_x = 0;
// uint16_t cursor_y = 0;
// uint16_t cursor_visible = 1;
// uint16_t t_width = 0;
// uint16_t t_height = 0;
// framebuffer_info_t *fbcli = 0;

// #define KEY_UP 0x80
// #define KEY_DOWN 0x81
// #define KEY_LEFT 0x82
// #define KEY_RIGHT 0x83

// int read_ps2_key()
// {
// 	uint8_t scancode = 0;
// 	bool is_extended = false;

// 	while (1)
// 	{
// 		// Чекаємо, поки з'являться дані
// 		while (!(inb(0x64) & 1))
// 			;

// 		scancode = inb(0x60);

// 		if (scancode == 0xE0)
// 		{
// 			is_extended = true;
// 			while (!(inb(0x64) & 1))
// 				;
// 			scancode = inb(0x60);
// 		}

// 		if (scancode & 0x80)
// 			continue;

// 		break;
// 	}

// 	if (is_extended)
// 	{
// 		switch (scancode)
// 		{
// 		case 0x48:
// 			return KEY_UP;
// 		case 0x50:
// 			return KEY_DOWN;
// 		case 0x4B:
// 			return KEY_LEFT;
// 		case 0x4D:
// 			return KEY_RIGHT;
// 		default:
// 			return 0;
// 		}
// 	}

// 	return scancode_to_ascii[scancode];
// }

// void read_line(char *buf, int max_len)
// {
// 	int buf_it = *bufer_history;
// 	int i = 0;
// 	while (i < max_len - 1)
// 	{
// 		int c = read_ps2_key();
// 		if ((uint8_t)c == KEY_UP)
// 		{
// 			handle_key_up(buf, &i, &buf_it);
// 			continue;
// 		}
// 		if ((uint8_t)c == KEY_DOWN)
// 		{
// 			handle_key_down(buf, &i, &buf_it);
// 			continue;
// 		}
// 		if (c == '\b')
// 		{
// 			handle_backspace(&i);
// 			continue;
// 		}

// 		if (c == '\n')
// 		{
// 			print("\n");
// 			break;
// 		}

// 		handle_regular_char(c, buf, &i);
// 	}
// 	buf[i] = '\0';
// }

// typedef enum
// {
// 	CMD_UNKNOWN,
// 	CMD_EXIT,
// 	CMD_HELLO,
// 	CMD_NEOFETCH,
// 	CMD_CLEAR,
// 	CMD_CHANGE_X,
// 	CMD_CHANGE_Y,
// 	CMD_HELP
// } Command;

// Command command_from_string(const char *input)
// {

// 	if (strcmp(input, "exit\0") == 0)
// 		return CMD_EXIT;
// 	if (strcmp(input, "hello") == 0)
// 		return CMD_HELLO;
// 	if (strcmp(input, "neofetch") == 0)
// 		return CMD_NEOFETCH;
// 	if (strcmp(input, "clear") == 0)
// 		return CMD_CLEAR;
// 	if (strcmp(input, "help") == 0)
// 		return CMD_HELP;
// 	return CMD_UNKNOWN;
// }

// void handle_help()
// {
// 	print("\nCommands:\n");
// 	print("  exit\n");
// 	print("  hello\n");
// 	print("  neofetch\n");
// 	print("  clear\n");
// 	print("  help\n");
// }

// int cli(framebuffer_info_t *fb)
// {
// 	fbcli = fb;
// 	uint32_t *pixels = (uint32_t *)fb->base;

// 	handle_clear();
// 	handle_neofetch();

// 	for (int i = 0; i < 50; i++)
// 	{
// 		key_history[i] = kmalloc(100);
// 		key_history[i][0] = '\0';
// 		if (key_history[i] == NULL)
// 		{
// 			print("Out of memory!\n");
// 			return 0;
// 		}
// 	}

// 	bufer_history = kmalloc(sizeof(int));
// 	if (bufer_history != NULL)
// 	{
// 		*bufer_history = 0;
// 	}
// 	while (1)
// 	{
// 		t_col = 0;
// 		print("> ", COLOR_GREEN);
// 		read_line(key_history[*bufer_history], 100);
// 		const char *input = key_history[*bufer_history];
// 		if (input[0] == '\0' || is_blank(input))
// 		{
// 			print("\n");
// 			continue;
// 		}

// 		++*bufer_history;

// 		Command cmd = command_from_string(input);

// 		switch (cmd)
// 		{
// 		case CMD_EXIT:
// 			handle_exit();
// 			return 0;
// 		case CMD_HELLO:
// 			handle_hello();
// 			break;
// 		case CMD_CLEAR:
// 			handle_clear();
// 			break;
// 		case CMD_NEOFETCH:
// 			handle_neofetch();
// 			break;
// 		case CMD_HELP:
// 			handle_help();
// 			break;
// 		case CMD_CHANGE_X:
// 			handle_change_x();
// 			break;
// 		case CMD_CHANGE_Y:
// 			handle_change_y();
// 			break;

// 		default:
// 			print("\nUnknown command!\n");
// 			break;
// 		}
// 	}
// }
