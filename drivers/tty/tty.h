/**
 * @file tty.h
 * @brief TTY abstraction — canonical line-discipline, console ops, read/write
 */
#pragma once

#include <smp/waitqueue.h>
#include <stdbool.h>
#include <stdint.h>

/* -- Console output abstraction -------------------------- */

/*
 * The TTY does not know about VGA or framebuffer details — it
 * calls into console_ops provided by the platform driver.
 */
typedef struct {
  void (*putchar)(char c);
  void (*clear)(void);
} tty_console_ops_t;

/* -- Buffer sizes ----------------------------------------- */

#define TTY_LINE_BUF_SIZE 256
#define TTY_READ_BUF_SIZE 4096

/* -- TTY instance ----------------------------------------- */

typedef struct {
  char line_buf[TTY_LINE_BUF_SIZE];
  size_t line_len;

  char read_buf[TTY_READ_BUF_SIZE];
  volatile size_t read_head;
  volatile size_t read_tail;

  const tty_console_ops_t *console;

  wait_queue_t read_wq;
} tty_t;

/* -- API -------------------------------------------------- */

/**
 * @brief Initialise a TTY instance
 * @param tty      Pointer to the TTY to initialise
 * @param console  Console operations (putchar, clear)
 */
void tty_init(tty_t *tty, const tty_console_ops_t *console);

/**
 * @brief Feed a character into the TTY line discipline
 * @param tty  Target TTY
 * @param c    Incoming character
 */
void tty_input_char(tty_t *tty, char c);

/**
 * @brief Blocking read — returns one canonical line at a time
 * @param tty   Source TTY
 * @param buf   Destination buffer
 * @param size  Maximum bytes to read
 * @return Number of bytes actually read
 */
size_t tty_read(tty_t *tty, char *buf, size_t size);

/**
 * @brief Write characters to the console via the TTY
 * @param tty   Target TTY
 * @param buf   Source buffer
 * @param size  Number of bytes to write
 * @return Number of bytes actually written
 */
size_t tty_write(tty_t *tty, const char *buf, size_t size);

extern tty_t *tty_current;
