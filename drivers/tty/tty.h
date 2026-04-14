#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <smp/waitqueue.h>

/* ── Console output abstraction ──────────────────────────────────────────── */
/*
 * tty не знає про VGA/framebuffer — він викликає console_ops.
 * Конкретний драйвер (vga_console, fb_console) реєструє свої ops.
 */
typedef struct
{
	void (*putchar)(char c);
	void (*clear)(void); /* CTRL+L */
} tty_console_ops_t;

/* ── TTY buffer sizes ────────────────────────────────────────────────────── */
#define TTY_LINE_BUF_SIZE 256  /* рядок що редагується зараз       */
#define TTY_READ_BUF_SIZE 4096 /* готові рядки чекають на read()   */

/* ── TTY struct ──────────────────────────────────────────────────────────── */
typedef struct
{
	/* --- line buffer (canonical mode) ---
	 * Сюди йдуть символи поки не прийде \n.
	 * Backspace видаляє звідси.
	 */
	char line_buf[TTY_LINE_BUF_SIZE];
	size_t line_len;

	/* --- read buffer ---
	 * Завершені рядки (після \n) чекають тут на read().
	 * Ring buffer.
	 */
	char read_buf[TTY_READ_BUF_SIZE];
	volatile size_t read_head;
	volatile size_t read_tail;

	/* --- output --- */
	const tty_console_ops_t *console;

	/* --- waitqueue для read() --- */
	wait_queue_t read_wq;
} tty_t;

/* ── API ─────────────────────────────────────────────────────────────────── */

/* Ініціалізація */
void tty_init(tty_t *tty, const tty_console_ops_t *console);

/* Надходить символ з клавіатури (викликається з tty keyboard handler) */
void tty_input_char(tty_t *tty, char c);

/* read() syscall — блокуючий, чекає на завершений рядок */
size_t tty_read(tty_t *tty, char *buf, size_t size);

/* write() syscall — пише в console */
size_t tty_write(tty_t *tty, const char *buf, size_t size);

/* Глобальний tty (одна консоль поки що) */
extern tty_t *tty_current;
