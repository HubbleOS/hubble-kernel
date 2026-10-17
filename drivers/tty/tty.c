/**
 * @file tty.c
 * @brief TTY core — termios line discipline (canonical and raw), ioctls
 */
#include <drivers/tty/tty.h>
#include <hubble/errno.h>
#include <hubble/string.h>
#include <smp/scheduler.h>
#include <smp/task.h>
#include <smp/waitqueue.h>

/* The console's tty, which the keyboard types into (set by tty_dev.c). */
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
  memset(tty, 0, sizeof(*tty));
  tty->console = console;
  tty->winsize = (tty_winsize_t){.ws_row = 25, .ws_col = 80};

  /* What Linux gives a fresh console: cooked input with echo. */
  memset(&tty->termios, 0, sizeof(tty->termios));
  tty->termios.c_iflag = TTY_ICRNL | TTY_IXON;
  tty->termios.c_oflag = TTY_OPOST | TTY_ONLCR;
  tty->termios.c_cflag = TTY_CFLAG_DEFAULT;
  tty->termios.c_lflag = TTY_ISIG | TTY_ICANON | TTY_ECHO | TTY_ECHOE |
                         TTY_ECHOK | TTY_ECHOCTL | TTY_ECHOKE | TTY_IEXTEN;
  tty->termios.c_cc[TTY_VINTR] = 0x03;  /* ^C */
  tty->termios.c_cc[TTY_VQUIT] = 0x1C;  /* ^\ */
  tty->termios.c_cc[TTY_VERASE] = 0x7F; /* DEL, what Backspace sends */
  tty->termios.c_cc[TTY_VKILL] = 0x15;  /* ^U */
  tty->termios.c_cc[TTY_VEOF] = 0x04;   /* ^D */
  tty->termios.c_cc[TTY_VMIN] = 1;

  waitqueue_init(&tty->read_wq);
}

/* -- Line discipline -------------------------------------- */

static void console_put(tty_t *tty, char c) {
  if (tty->console && tty->console->putchar)
    tty->console->putchar(tty, c);
}

/* Echo one input byte; control characters as ^X with ECHOCTL (so an
 * arrow key in a cooked shell shows "^[[A", not a cursor jump). */
static void echo_char(tty_t *tty, char c) {
  uint32_t lflag = tty->termios.c_lflag;
  if (!(lflag & TTY_ECHO))
    return;
  unsigned char u = (unsigned char)c;
  if ((u < 0x20 && c != '\n' && c != '\t') || u == 0x7F) {
    if (lflag & TTY_ECHOCTL) {
      console_put(tty, '^');
      console_put(tty, (char)(u ^ 0x40));
    }
    return;
  }
  console_put(tty, c);
}

static void erase_one(tty_t *tty) {
  if (tty->line_len == 0)
    return;
  tty->line_len--;
  if ((tty->termios.c_lflag & (TTY_ECHO | TTY_ECHOE)) ==
      (TTY_ECHO | TTY_ECHOE)) {
    console_put(tty, '\b');
    console_put(tty, ' ');
    console_put(tty, '\b');
  }
}

/* Make the edited line readable (canonical mode). */
static void flush_line(tty_t *tty) {
  for (size_t i = 0; i < tty->line_len; i++)
    read_buf_push(tty, tty->line_buf[i]);
  tty->line_len = 0;
}

void tty_input_char(tty_t *tty, char c) {
  if (!tty)
    return;

  const tty_termios_t *t = &tty->termios;

  /* Input mapping: a terminal's Enter sends CR; ICRNL makes it NL. */
  if (c == '\r') {
    if (t->c_iflag & TTY_IGNCR)
      return;
    if (t->c_iflag & TTY_ICRNL)
      c = '\n';
  } else if (c == '\n' && (t->c_iflag & TTY_INLCR)) {
    c = '\r';
  }

  /* Raw (non-canonical): every byte is readable at once. */
  if (!(t->c_lflag & TTY_ICANON)) {
    read_buf_push(tty, c);
    echo_char(tty, c);
    waitqueue_wake_all(&tty->read_wq);
    return;
  }

  /* Canonical: edit a line, hand it over on newline or EOF. */
  if (c == (char)t->c_cc[TTY_VERASE] || c == '\b') {
    erase_one(tty);
    return;
  }
  if (c == (char)t->c_cc[TTY_VKILL]) {
    while (tty->line_len)
      erase_one(tty);
    return;
  }
  if (c == (char)t->c_cc[TTY_VEOF]) {
    if (tty->line_len == 0)
      tty->eof_pending = true;
    flush_line(tty);
    waitqueue_wake_all(&tty->read_wq);
    return;
  }
  if (c == '\f' && tty->console && tty->console->clear) {
    tty->console->clear(tty);
    return;
  }

  /* Keep room for the newline so a full line can still be ended. */
  if (c != '\n' && tty->line_len >= TTY_LINE_BUF_SIZE - 1)
    return;

  echo_char(tty, c);
  tty->line_buf[tty->line_len++] = c;

  if (c == '\n') {
    flush_line(tty);
    waitqueue_wake_all(&tty->read_wq);
  }
}

void tty_input_string(tty_t *tty, const char *s) {
  while (*s)
    tty_input_char(tty, *s++);
}

/* -- Blocking read ---------------------------------------- */

bool tty_readable(tty_t *tty) {
  return !read_buf_empty(tty) || tty->eof_pending || tty->hung_up;
}

bool tty_input_room(tty_t *tty) {
  /* Room for a whole line to be handed over, so it can't stall midway. */
  return read_buf_len(tty) + TTY_LINE_BUF_SIZE < TTY_READ_BUF_SIZE - 1;
}

static bool tty_readable_cond(void *arg) { return tty_readable(arg); }

size_t tty_read(tty_t *tty, char *buf, size_t size) {
  if (!tty || !buf || size == 0)
    return -1;

  bool canonical = tty->termios.c_lflag & TTY_ICANON;

  /* Raw with VMIN 0 is a non-blocking read. */
  if (!canonical && tty->termios.c_cc[TTY_VMIN] == 0 && !tty_readable(tty))
    return 0;

  waitqueue_wait_event(&tty->read_wq, tty_readable_cond, tty, 0);

  if (read_buf_empty(tty) && tty->eof_pending) {
    tty->eof_pending = false;
    return 0;
  }
  if (read_buf_empty(tty))
    return 0; /* hung up */

  size_t n = 0;
  while (n < size && !read_buf_empty(tty)) {
    buf[n] = read_buf_pop(tty);
    if (canonical && buf[n++] == '\n')
      break;
    if (!canonical)
      n++;
  }
  waitqueue_wake_all(&tty->read_wq); /* room for a waiting writer */
  return n;
}

/* -- ioctl ------------------------------------------------ */

static void flush_input(tty_t *tty) {
  tty->read_tail = tty->read_head;
  tty->line_len = 0;
  tty->eof_pending = false;
}

static void set_termios(tty_t *tty, const tty_termios_t *next) {
  bool was_canonical = tty->termios.c_lflag & TTY_ICANON;
  tty->termios = *next;
  /* Leaving canonical mode: a half-typed line becomes raw input. */
  if (was_canonical && !(next->c_lflag & TTY_ICANON)) {
    flush_line(tty);
    waitqueue_wake_all(&tty->read_wq);
  }
}

long tty_set_ctty(tty_t *tty, bool steal) {
  task_t *task = get_current_task();
  if (task->sid != task->id.pid)
    return -EPERM; /* only a session leader takes a terminal */
  if (tty->session == task->sid)
    return 0;
  if (tty->session && !steal)
    return -EPERM;
  tty->session = task->sid;
  tty->pgrp = task->pgid;
  return 0;
}

/* The job-control ioctls only work on the caller's own terminal. */
static bool is_ctty(tty_t *tty) {
  return tty->session && tty->session == get_current_task()->sid;
}

long tty_ioctl(tty_t *tty, unsigned long request, void *arg) {
  switch (request) {
  case TTY_TCGETS:
    if (!arg)
      return -EFAULT;
    memcpy(arg, &tty->termios, sizeof(tty->termios));
    return 0;

  case TTY_TCSETSF:
    flush_input(tty);
    /* fall through */
  case TTY_TCSETS:
  case TTY_TCSETSW: {
    if (!arg)
      return -EFAULT;
    tty_termios_t next;
    memcpy(&next, arg, sizeof(next));
    set_termios(tty, &next);
    return 0;
  }

  case TTY_TCFLSH: /* arg is the queue selector, passed by value */
    if ((uintptr_t)arg == 0 || (uintptr_t)arg == 2)
      flush_input(tty);
    return 0;

  case TTY_TIOCGWINSZ: {
    if (!arg)
      return -EFAULT;
    tty_winsize_t ws = tty->winsize;
    if (tty->console && tty->console->get_size)
      tty->console->get_size(tty, &ws.ws_col, &ws.ws_row);
    memcpy(arg, &ws, sizeof(ws));
    return 0;
  }

  case TTY_TIOCSWINSZ: /* a console with a size of its own ignores it */
    if (!arg)
      return -EFAULT;
    memcpy(&tty->winsize, arg, sizeof(tty->winsize));
    return 0;

  case TTY_TIOCSCTTY: /* arg 1: take it from another session */
    return tty_set_ctty(tty, (uintptr_t)arg == 1);

  case TTY_TIOCNOTTY:
    if (!is_ctty(tty))
      return -ENOTTY;
    tty->session = tty->pgrp = 0;
    return 0;

  case TTY_TIOCGPGRP:
    if (!arg)
      return -EFAULT;
    if (!is_ctty(tty))
      return -ENOTTY;
    *(int *)arg = (int)tty->pgrp;
    return 0;

  case TTY_TIOCSPGRP:
    if (!arg)
      return -EFAULT;
    if (!is_ctty(tty))
      return -ENOTTY;
    if (*(int *)arg <= 0)
      return -EINVAL;
    tty->pgrp = (uint32_t)*(int *)arg;
    return 0;

  case TTY_TIOCGSID:
    if (!arg)
      return -EFAULT;
    if (!tty->session)
      return -ENOTTY;
    *(int *)arg = (int)tty->session;
    return 0;

  /* A graphical program takes the display (KD_GRAPHICS) so console
   * output stops drawing over it, and gives it back with KD_TEXT. */
  case TTY_KDSETMODE: { /* arg is the mode, passed by value */
    uintptr_t mode = (uintptr_t)arg;
    if (mode != TTY_KD_TEXT && mode != TTY_KD_GRAPHICS)
      return -EINVAL;
    if (tty->console && tty->console->set_output)
      tty->console->set_output(tty, mode == TTY_KD_TEXT);
    return 0;
  }

  case TTY_KDGETMODE: {
    if (!arg)
      return -EFAULT;
    bool text = !tty->console || !tty->console->output_enabled ||
                tty->console->output_enabled(tty);
    *(int *)arg = text ? TTY_KD_TEXT : TTY_KD_GRAPHICS;
    return 0;
  }

  case TTY_FIONREAD:
    if (!arg)
      return -EFAULT;
    *(int *)arg = (int)read_buf_len(tty);
    return 0;

  default:
    return -ENOTTY;
  }
}

/* -- Write to console ------------------------------------- */

size_t tty_write(tty_t *tty, const char *buf, size_t size) {
  if (!tty || !buf || !tty->console || !tty->console->putchar)
    return -1;

  if (tty->console->write) {
    tty->console->write(tty, buf, size);
    return size;
  }
  for (size_t i = 0; i < size; i++)
    tty->console->putchar(tty, buf[i]);

  return (size_t)size;
}
