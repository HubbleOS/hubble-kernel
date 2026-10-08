/**
 * @file tty.h
 * @brief TTY abstraction — termios line discipline, console ops, read/write
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
  /* Text grid size, for TIOCGWINSZ. Optional (80x25 if NULL). */
  void (*get_size)(uint16_t *cols, uint16_t *rows);
  /* Display ownership, for KDSETMODE/KDGETMODE. Optional. */
  void (*set_output)(bool enabled);
  bool (*output_enabled)(void);
} tty_console_ops_t;

/* -- termios (Linux x86_64 kernel ABI for TCGETS/TCSETS) -- */

typedef struct {
  uint32_t c_iflag;
  uint32_t c_oflag;
  uint32_t c_cflag;
  uint32_t c_lflag;
  uint8_t c_line;
  uint8_t c_cc[19];
} tty_termios_t;

/* c_iflag */
#define TTY_INLCR 0x0040
#define TTY_IGNCR 0x0080
#define TTY_ICRNL 0x0100
#define TTY_IXON 0x0400
/* c_oflag */
#define TTY_OPOST 0x0001
#define TTY_ONLCR 0x0004
/* c_cflag: B38400 | CS8 | CREAD */
#define TTY_CFLAG_DEFAULT 0x00BF
/* c_lflag */
#define TTY_ISIG 0x0001
#define TTY_ICANON 0x0002
#define TTY_ECHO 0x0008
#define TTY_ECHOE 0x0010
#define TTY_ECHOK 0x0020
#define TTY_ECHOCTL 0x0200
#define TTY_ECHOKE 0x0800
#define TTY_IEXTEN 0x8000
/* c_cc indices */
#define TTY_VINTR 0
#define TTY_VQUIT 1
#define TTY_VERASE 2
#define TTY_VKILL 3
#define TTY_VEOF 4
#define TTY_VTIME 5
#define TTY_VMIN 6

/* ioctl requests */
#define TTY_TCGETS 0x5401
#define TTY_TCSETS 0x5402
#define TTY_TCSETSW 0x5403
#define TTY_TCSETSF 0x5404
#define TTY_TCFLSH 0x540B
#define TTY_TIOCGWINSZ 0x5413
#define TTY_TIOCSWINSZ 0x5414
#define TTY_KDSETMODE 0x4B3A
#define TTY_KDGETMODE 0x4B3B

/* KDSETMODE modes: who draws on the display */
#define TTY_KD_TEXT 0x00
#define TTY_KD_GRAPHICS 0x01
#define TTY_FIONREAD 0x541B

/* -- Buffer sizes ----------------------------------------- */

#define TTY_LINE_BUF_SIZE 256
#define TTY_READ_BUF_SIZE 4096

/* -- TTY instance ----------------------------------------- */

typedef struct {
  /* Canonical mode: the line being edited, not yet readable. */
  char line_buf[TTY_LINE_BUF_SIZE];
  size_t line_len;

  /* Bytes ready for read(): whole lines (canonical) or raw keys. */
  char read_buf[TTY_READ_BUF_SIZE];
  volatile size_t read_head;
  volatile size_t read_tail;
  /* VEOF on an empty line: the next read() returns 0. */
  volatile bool eof_pending;

  tty_termios_t termios;
  const tty_console_ops_t *console;

  wait_queue_t read_wq;
} tty_t;

/* -- API -------------------------------------------------- */

/**
 * @brief Initialise a TTY instance (canonical mode, echo on)
 * @param tty      Pointer to the TTY to initialise
 * @param console  Console operations
 */
void tty_init(tty_t *tty, const tty_console_ops_t *console);

/**
 * @brief Feed one byte of keyboard input through the line discipline
 * @param tty  Target TTY
 * @param c    Incoming byte, as a terminal sends it (Enter is '\r')
 */
void tty_input_char(tty_t *tty, char c);

/** @brief Feed a key's byte sequence (e.g. "\x1b[A" for Up). */
void tty_input_string(tty_t *tty, const char *s);

/**
 * @brief Blocking read: one line (canonical) or the bytes available (raw)
 * @return Bytes read; 0 at end of input (VEOF on an empty line)
 */
size_t tty_read(tty_t *tty, char *buf, size_t size);

/** @brief Whether a read() would return without blocking. */
bool tty_readable(tty_t *tty);

/**
 * @brief termios / window-size ioctls
 * @return 0 or a value on success, negative errno on failure
 */
long tty_ioctl(tty_t *tty, unsigned long request, void *arg);

/**
 * @brief Write characters to the console via the TTY
 * @return Number of bytes actually written
 */
size_t tty_write(tty_t *tty, const char *buf, size_t size);

extern tty_t *tty_current;
