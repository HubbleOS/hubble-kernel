// user_main.c
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "libc.h"
#include <stdio.h>

#include <sys/syscall.h>

void *mmap_(uint64_t addr, size_t length, int prot, int flags,
	    int fd, uint64_t offset)
{
	return (uint64_t *)syscall6(3, addr, length, prot, flags, fd, offset);
}

int read_file(int fd, void *buf, size_t size) { return syscall3(2, fd, (long)buf, size); }

int write_file(int fd, void *buf, size_t size) { return syscall3(1, fd, (long)buf, size); }

int lseek(int fd, uint64_t offset, int whence) { return syscall3(8, fd, offset, whence); }

int spawn(void *entry_point, void *arg, uint32_t priority) { return syscall3(6, (long)entry_point, (long)arg, priority); }

typedef struct
{
	int32_t x, y;
	bool left, right, middle; // current held state
	bool left_clicked;	  // set on press, you clear it after handling
	bool right_clicked;
} mouse_t;

uint32_t open(const char *path, int flags) { return syscall2(4, (long)path, flags); }

void test(void);
void handle_command(char *cmd);

// void terminal(void)
// {
// 	char buf[128];
// 	int pos = 0;
// 	char c;

// 	int kbd_fd = open("/dev/kbd", 0);
// 	int term_fd = open("/pipe/term", 0);

// 	printf("SimpleOS Terminal\n> ");

// 	while (1)
// 	{
// 		read_file(kbd_fd, &c, 1);

// 		if (c == '\n' || c == '\r') // Enter
// 		{
// 			buf[pos] = '\0';
// 			lseek(term_fd, 0, 0);
// 			write_file(term_fd, buf, pos);
// 			printf("\n");	     // переход на новую строку
// 			handle_command(buf); // обработка команд
// 			printf("> ");
// 			pos = 0;
// 		}
// 		else if (c == '\b' && pos > 0) // Backspace
// 		{
// 			pos--;
// 			printf("\b \b");
// 		}
// 		else
// 		{
// 			buf[pos++] = c;
// 			printf("%c", c);
// 		}
// 	}
// }

void terminal(void)
{
	char buf[256];
	int tty_fd = open("/dev/tty0", 0);

	printf("> ");
	while (1)
	{
		// read() блокується поки не прийде \n
		// ядро вже зробило echo і backspace
		int n = read_file(tty_fd, buf, sizeof(buf));
		if (n <= 0)
			continue;
		buf[n] = '\0';

		handle_command(buf);
		printf("> ");
	}
}

void handle_command(char *cmd)
{
	int len = strlen(cmd);
	while (len > 0 && (cmd[len - 1] == '\n' || cmd[len - 1] == '\r'))
	{
		cmd[--len] = '\0';
	}

	if (strcmp(cmd, "help") == 0)
	{
		printf("Available commands: help, echo, clear\n");
	}
	else if (strncmp(cmd, "echo ", 5) == 0)
	{
		printf("%s\n", cmd + 5);
	}
	else
	{
		printf("Unknown command: %s\n", cmd);
	}
}

void _start(void)
{
	libc_init();
	printf("Hello from user space 2!\n");
	// int pid = spawn(test, NULL, 0);
	int pid2 = spawn(terminal, NULL, 0);
	while (1)
	{
		;
	}
}
void test(void)
{
	printf("Test task 2!\n");
	char c;
	char buf[128];
	int pos = 0;
	int kbd_file = open("/dev/kbd", 0);
	int pipe_file = open("/pipe/term", 0);
	while (1)
	{
		;
		read_file(kbd_file, &c, 1);
		buf[pos++] = c;
		lseek(pipe_file, 0, 0);
		write_file(pipe_file, buf, pos);
		pos = 0;
		// printf("%s", buf);
		// printf("key 2: %c\n", c);
	};
	// printf("Test task!\n")
}

// #include <stddef.h>
// #include <stdint.h>
// #include <stdbool.h>
// #include <string.h>
// #include <stdlib.h>

// #include "libc.h"
// #include <stdio.h>
// #include <sys/syscall.h>

// #include <libgui/core.h>
// #include <libgui/ui.h>
// #include <dev/mouse/mouse.h>

// /* ── syscalls ────────────────────────────────────────────────────────────── */

// void *mmap_(uint64_t addr, size_t len, int prot, int flags, int fd, uint64_t off)
// {
// 	return (void *)syscall6(3, addr, len, prot, flags, fd, off);
// }
// int read_file(int fd, void *buf, size_t size) { return syscall3(SYS_read, fd, (long)buf, size); }
// int write_file(int fd, void *buf, size_t size) { return syscall3(1, fd, (long)buf, size); }
// uint32_t open(const char *path, int flags) { return syscall2(SYS_open, (long)path, flags); }
// int spawn(void *fn, void *arg, uint32_t pri) { return syscall3(6, (long)fn, (long)arg, pri); }

// /* ── Термінальний стан ───────────────────────────────────────────────────── */

// #define TERM_COLS 60
// #define TERM_ROWS 20
// #define TERM_LINE_H 14 /* висота рядка в пікселях */
// #define TERM_PAD_X 8
// #define TERM_PAD_Y 8

// typedef struct
// {
// 	char lines[TERM_ROWS][TERM_COLS + 1]; /* буфер рядків             */
// 	int cur_row;			      /* поточний рядок           */
// 	int cur_col;			      /* поточна колонка          */
// 	label_t *labels[TERM_ROWS];	      /* один label на рядок      */
// 	window_t *win;
// } terminal_view_t;

// static terminal_view_t g_term;

// /* ── Ініціалізація вікна терміналу ──────────────────────────────────────── */

// static void terminal_view_init(void)
// {
// 	int win_w = TERM_COLS * 8 + TERM_PAD_X * 2;		   /* ~8px на символ       */
// 	int win_h = TERM_ROWS * TERM_LINE_H + TERM_PAD_Y * 2 + 20; /* +titlebar */

// 	g_term.win = window_create(50, 50, win_w, win_h);
// 	g_term.cur_row = 0;
// 	g_term.cur_col = 0;

// 	for (int i = 0; i < TERM_ROWS; i++)
// 	{
// 		memset(g_term.lines[i], 0, sizeof(g_term.lines[i]));
// 		g_term.labels[i] = label_create(
// 		    TERM_PAD_X,
// 		    TERM_PAD_Y + i * TERM_LINE_H,
// 		    win_w - TERM_PAD_X * 2,
// 		    TERM_LINE_H,
// 		    "");
// 		window_addElement(g_term.win, g_term.labels[i]);
// 	}
// }

// /* ── Прокрутка ───────────────────────────────────────────────────────────── */

// static void terminal_scroll(void)
// {
// 	/* зсуваємо рядки вгору */
// 	for (int i = 0; i < TERM_ROWS - 1; i++)
// 	{
// 		memcpy(g_term.lines[i], g_term.lines[i + 1], TERM_COLS + 1);
// 		label_set_text(g_term.labels[i], g_term.lines[i]);
// 	}
// 	memset(g_term.lines[TERM_ROWS - 1], 0, TERM_COLS + 1);
// 	label_set_text(g_term.labels[TERM_ROWS - 1], "");
// 	g_term.cur_row = TERM_ROWS - 1;
// 	g_term.cur_col = 0;
// }

// /* ── Вивести один символ ─────────────────────────────────────────────────── */

// static void terminal_putchar(char c)
// {
// 	if (c == '\n' || g_term.cur_col >= TERM_COLS)
// 	{
// 		/* оновлюємо label поточного рядка */
// 		label_set_text(g_term.labels[g_term.cur_row], g_term.lines[g_term.cur_row]);

// 		g_term.cur_row++;
// 		g_term.cur_col = 0;

// 		if (g_term.cur_row >= TERM_ROWS)
// 			terminal_scroll();
// 		return;
// 	}

// 	if (c == '\b')
// 	{
// 		if (g_term.cur_col > 0)
// 		{
// 			g_term.cur_col--;
// 			g_term.lines[g_term.cur_row][g_term.cur_col] = ' ';
// 			g_term.lines[g_term.cur_row][g_term.cur_col + 1] = '\0';
// 			label_set_text(g_term.labels[g_term.cur_row], g_term.lines[g_term.cur_row]);
// 		}
// 		return;
// 	}

// 	if (c < 0x20)
// 		return; /* ігноруємо інші керуючі символи */

// 	g_term.lines[g_term.cur_row][g_term.cur_col] = c;
// 	g_term.lines[g_term.cur_row][g_term.cur_col + 1] = '\0';
// 	label_set_text(g_term.labels[g_term.cur_row], g_term.lines[g_term.cur_row]);
// 	g_term.cur_col++;
// }

// /* ── Читання з pipe і вивід у вікно ─────────────────────────────────────── */

// static void terminal_reader(void)
// {
// 	int pipe_fd = open("/pipe/tty0_out", 0);
// 	char buf[64];

// 	while (1)
// 	{
// 		int n = read_file(pipe_fd, buf, sizeof(buf));
// 		if (n <= 0)
// 			continue;
// 		for (int i = 0; i < n; i++)
// 			terminal_putchar(buf[i]);
// 	}
// }

// /* ── Main ────────────────────────────────────────────────────────────────── */

// typedef struct
// {
// 	int32_t x, y;
// 	bool left, right, middle; // current held state
// 	bool left_clicked;	  // set on press, you clear it after handling
// 	bool right_clicked;
// } mouse_t;

// void _start(void)
// {
// 	libc_init();

// 	/* framebuffer */
// 	int fb_file = open("/dev/fb0", 0);
// 	uint32_t *fb = (uint32_t *)mmap_(0, 800 * 5120, 3, 1, fb_file, 0);

// 	int mouse_file = open("/dev/mouse", 0);
// 	mouse_t *mouse = (mouse_t *)mmap_(0, sizeof(mouse_t), 3, 1, mouse_file, 0);

// 	framebuffer_info_t fb_info = {
// 	    .base = fb,
// 	    .width = 1280,
// 	    .height = 800,
// 	    .pitch = 5120,
// 	    .bpp = 32,
// 	};

// 	screen_init(&fb_info);
// 	compositor_init();
// 	background_create(rgb(20, 20, 20));

// 	cursor_t *cursor = cursor_create(16, 16, rgb(0, 0, 0), rgb(255, 255, 255));

// 	/* термінальне вікно */
// 	terminal_view_init();

// 	/* читач pipe — окремий таск */
// 	spawn(terminal_reader, NULL, 0);

// 	/* render loop */
// 	while (1)
// 	{
// 		mouse_update(mouse->x, mouse->y, mouse->left);

// 		if (cursor->surface->x != mouse->x || cursor->surface->y != mouse->y)
// 			cursor_move(cursor, mouse->x, mouse->y);

// 		if (g_mouse.drag_obj)
// 		{
// 			int dx = g_mouse.x - g_mouse.drag_offset_x;
// 			int dy = g_mouse.y - g_mouse.drag_offset_y;
// 			if (g_mouse.drag_el)
// 				object_move_element(g_mouse.drag_obj, g_mouse.drag_el,
// 						    dx - g_mouse.drag_obj->x,
// 						    dy - g_mouse.drag_obj->y);
// 			else
// 			{
// 				compositor_move_object(g_mouse.drag_obj, dx, dy);
// 				compositor_bring_to_front(g_mouse.drag_obj, LAYER_WINDOWS);
// 			}
// 		}

// 		compositor_render();
// 	}
// }
