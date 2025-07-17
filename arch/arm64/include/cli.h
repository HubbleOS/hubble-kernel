#pragma once

#include <stdint.h>

extern void _terminate(void);

#define ESCAPE 0x1b

#define ARROW_UP "\x1b[A"
#define ARROW_DOWN "\x1b[B"
#define ARROW_LEFT "\x1b[D"
#define ARROW_RIGHT "\x1b[C"
#define ENTER "\x1b[1;5A"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

volatile uint8_t *uart = (uint8_t *)0x09000000;

void putchar(char c) { *uart = c; }

void print(const char *s)
{
	while (*s != '\0')
		putchar(*s++);
}

#define UART_DR (*(volatile uint8_t *)0x09000000)
#define UART_FR (*(volatile uint32_t *)0x09000018) // UART Flag Register

#define IS_BACKSPACE(c) (c == 0x08 || c == 0x7F)

char getchar(void)
{
	while (*(volatile uint32_t *)0x09000018 & (1 << 4))
		;
	return UART_DR;
}

#define END_OF_TEXT 0x03		 // CTRL+C
#define END_OF_TRANSMISSION 0x04 // CTRL+D

void read_line(char *buf, int max_len)
{
	int i = 0;
	while (i < max_len - 1)
	{
		char c = getchar();

		if (c == END_OF_TRANSMISSION)
		{
			print("^D\r");
			buf[0] = '\0';
			return;
		}

		if (c == END_OF_TEXT)
		{
			print("^C\r");
			buf[0] = '\0';
			return;
		}

		if (IS_BACKSPACE(c))
		{
			if (i > 0)
			{
				i--;
				print("\b \b");
			}
			continue;
		}

		putchar(c);

		if (c == '\r' || c == '\n')
			break;

		buf[i++] = c;
	}
	buf[i] = '\0';
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

void print_dec(unsigned int num)
{
	char buf[10];
	int i = 0;

	if (num == 0)
	{
		putchar('0');
		return;
	}

	while (num > 0)
	{
		buf[i++] = '0' + (num % 10);
		num /= 10;
	}

	while (i--)
		putchar(buf[i]);
}

void neofetch(void)
{
	print(ANSI_COLOR_BLUE);
	print(
		"  _____                _              \n"
		" / ____|              (_)             \n"
		"| |       ___   _ __   _  _   _  _ __ ___\n"
		"| |      / _ \\ | '_ \\ | || | | || '_ ` _ \\\n"
		"| |____ | (_) || |_) || || |_| || | | | | |\n"
		" \\_____| \\___/ | .__/ |_| \\__,_||_| |_| |_|\n"
		"               | |                         \n"
		"               |_|                         \n"
		"\n");

	print(ANSI_COLOR_RESET);
}

void clearConsole(void)
{
	print("\x1b[2J");
	print("\x1b[H");
}

void cli(void)
{
	// clearConsole();
	neofetch();

	while (1)
	{
		print(ANSI_COLOR_GREEN);
		print("> ");
		print(ANSI_COLOR_RESET);

		char buffer[128];
		read_line(buffer, sizeof(buffer));

		if (buffer[0] == '\0')
		{
			print("\r\n");
			continue;
		}

		if (strcmp(buffer, "exit") == 0)
		{
			print("\r\nGoodbye!\n");
			_terminate();
			// break;
		}
		else if (strcmp(buffer, "hello") == 0)
		{
			print("\r\nHello!\n");
		}
		else if (strcmp(buffer, "neofetch") == 0)
		{
			neofetch();
		}
		else if (strcmp(buffer, "clear") == 0)
		{
			clearConsole();
		}
		else
		{
			print("\r\nUnknown command!\n");
		}

		print("\r");
	}
}