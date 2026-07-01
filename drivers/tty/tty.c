/**
 * @file tty.c
 * @brief TTY core — canonical line discipline, read/write, ring buffer
 */
#include <drivers/tty/tty.h>
#include <hubble/string.h>
#include <smp/waitqueue.h>

tty_t *tty_current = NULL;

/* -- Read ring-buffer helpers ----------------------------- */

static inline size_t read_buf_len(const tty_t *tty) {
  return (tty->read_head - tty->read_tail + TTY_READ_BUF_SIZE) %
         TTY_READ_BUF_SIZE;
}

static inline bool read_buf_empty(const tty_t *tty) {
  return tty->read_head == tty->read_tail;
}

static void read_buf_push(tty_t *tty, char c) {
  size_t next = (tty->read_head + 1) % TTY_READ_BUF_SIZE;
  if (next == tty->read_tail)
    return;
  tty->read_buf[tty->read_head] = c;
  tty->read_head = next;
}

static char read_buf_pop(tty_t *tty) {
  char c = tty->read_buf[tty->read_tail];
  tty->read_tail = (tty->read_tail + 1) % TTY_READ_BUF_SIZE;
  return c;
}

/* -- Initialisation --------------------------------------- */

void tty_init(tty_t *tty, const tty_console_ops_t *console) {
  tty->line_len = 0;
  tty->read_head = 0;
  tty->read_tail = 0;
  tty->console = console;

  waitqueue_init(&tty->read_wq);

  if (!tty_current)
    tty_current = tty;
}

/* -- Line discipline -------------------------------------- */

void tty_input_char(tty_t *tty, char c) {
  if (!tty)
    return;

  if (c == '\b') {
    if (tty->line_len > 0) {
      tty->line_len--;
      if (tty->console && tty->console->putchar) {
        tty->console->putchar('\b');
        tty->console->putchar(' ');
        tty->console->putchar('\b');
      }
    }
    return;
  }

  if (c == '\f') {
    if (tty->console && tty->console->clear)
      tty->console->clear();
    return;
  }

  if (tty->console && tty->console->putchar)
    tty->console->putchar(c);

  if (tty->line_len < TTY_LINE_BUF_SIZE - 1)
    tty->line_buf[tty->line_len++] = c;

  if (c == '\n') {
    for (size_t i = 0; i < tty->line_len; i++)
      read_buf_push(tty, tty->line_buf[i]);
    tty->line_len = 0;

    waitqueue_wake_all(&tty->read_wq);
  }
}

/* -- Blocking read ---------------------------------------- */

size_t tty_read(tty_t *tty, char *buf, size_t size) {
  if (!tty || !buf || size == 0)
    return -1;

  while (read_buf_empty(tty))
    waitqueue_sleep(&tty->read_wq);

  size_t n = 0;
  while (n < size && !read_buf_empty(tty)) {
    buf[n] = read_buf_pop(tty);
    if (buf[n++] == '\n')
      break;
  }

  return (size_t)n;
}

/* -- Write to console ------------------------------------- */

size_t tty_write(tty_t *tty, const char *buf, size_t size) {
  if (!tty || !buf || !tty->console || !tty->console->putchar)
    return -1;

  for (size_t i = 0; i < size; i++)
    tty->console->putchar(buf[i]);

  return (size_t)size;
}
