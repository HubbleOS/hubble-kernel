/**
 * @file pty.c
 * @brief Pseudo-terminals: /dev/ptmx and /dev/pts/N
 *
 * A pty is a terminal whose keyboard and screen are a program: a terminal
 * emulator, tmux, sshd. Opening /dev/ptmx makes a new pty and returns its
 * master side; the slave side, /dev/pts/N, is an ordinary terminal (the
 * same line discipline as the console: termios, echo, line editing) that
 * a shell runs on.
 *
 *   bytes written to the master  -> the slave's input (as if typed)
 *   bytes the slave writes       -> readable from the master (its screen)
 *
 * The usual dance (musl's openpty/forkpty does it): open /dev/ptmx,
 * unlock it (TIOCSPTLCK 0), ask its number (TIOCGPTN), open /dev/pts/N;
 * the child calls setsid() and makes the slave its controlling terminal.
 *
 * Closing the master hangs the slave up: its reads return end of file
 * and its writes fail. Closing every slave makes master reads fail with
 * EIO, which is how a terminal learns its shell is gone.
 */
#include <drivers/tty/tty.h>
#include <fs/vfs/dev.h>
#include <fs/vfs/file.h>
#include <fs/vfs/stream.h>
#include <hubble/errno.h>
#include <hubble/module.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <smp/spinlock.h>
#include <smp/waitqueue.h>

#define PTY_MAX 16
#define PTY_OUT_SIZE 16384 /* slave output not yet read by the master */

/* Linux ioctls on the master */
#define TIOCGPTN 0x80045430   /* the pty's number, N in /dev/pts/N */
#define TIOCSPTLCK 0x40045431 /* lock (1) or unlock (0) the slave */
#define TIOCGPTLCK 0x80045439

#define O_ACCMODE 0x3
#define O_NOCTTY 0x100
#define O_NONBLOCK 0x800

typedef struct {
  tty_t tty;          /* the slave's terminal; tty.read_wq is everyone's */
  stream_t out;       /* slave output, for the master to read */
  bool master_open;   /* this slot is in use while the master is open */
  bool locked;        /* the slave can't be opened until unlocked */
  bool slave_opened;  /* ever: before that, master reads just wait */
  int slave_count;    /* open slave files */
  int index;
} pty_t;

static pty_t *ptys[PTY_MAX];
/* Guards ptys[] and every pty's open state (master_open, locked,
 * slave_count). */
static spinlock_t ptys_lock = SPINLOCK_INIT("pty");

static int file_flags(int open_flags) {
  return open_flags & (O_ACCMODE | O_NONBLOCK);
}

static bool nonblock(file_t *file) { return file->flags & FILE_O_NONBLOCK; }

/* Free the pty once neither side has it open. Caller holds ptys_lock;
 * returns the pty to free after dropping it. */
static pty_t *pty_unused(pty_t *pty) {
  if (pty->master_open || pty->slave_count)
    return NULL;
  ptys[pty->index] = NULL;
  return pty;
}

static void pty_free(pty_t *pty) {
  if (!pty)
    return;
  stream_destroy(&pty->out);
  kfree(pty);
}

/* -- The slave's screen: the master's input ---------------- */

/* Output processing a real terminal expects: OPOST|ONLCR turns "\n"
 * into "\r\n". The console does that itself; a pty must. */
static void pty_output(tty_t *tty, const char *buf, size_t len,
                       bool can_wait) {
  pty_t *pty = tty->priv;
  bool onlcr = (tty->termios.c_oflag & (TTY_OPOST | TTY_ONLCR)) ==
               (TTY_OPOST | TTY_ONLCR);
  size_t start = 0;
  for (size_t i = 0; i <= len; i++) {
    if (i < len && !(onlcr && buf[i] == '\n'))
      continue;
    if (i > start &&
        stream_write(&pty->out, buf + start, i - start, !can_wait) < 0)
      return; /* master gone, or full while echoing */
    if (i < len && stream_write(&pty->out, "\r\n", 2, !can_wait) < 0)
      return;
    start = i + 1;
  }
}

/* Echo, from the master's write: never wait for the master's own read. */
static void pty_putchar(tty_t *tty, char c) { pty_output(tty, &c, 1, false); }

/* The slave writing: wait while the master's screen is behind. */
static void pty_write_out(tty_t *tty, const char *buf, size_t len) {
  pty_output(tty, buf, len, true);
}

static const tty_console_ops_t pty_console_ops = {
    .putchar = pty_putchar,
    .write = pty_write_out,
};

/* -- Master file ------------------------------------------- */

static bool master_readable(void *arg) {
  pty_t *pty = arg;
  return pty->out.count || (pty->slave_opened && !pty->slave_count);
}

static long master_read(file_t *file, void *buf, size_t len) {
  pty_t *pty = file->priv;
  if (!len)
    return 0;
  if (!master_readable(pty)) {
    if (nonblock(file))
      return -EAGAIN;
    waitqueue_wait_event(&pty->tty.read_wq, master_readable, pty, 0);
  }
  if (!pty->out.count)
    return -EIO; /* every slave closed: the shell is gone */
  return stream_read(&pty->out, buf, len, true);
}

static bool master_room(void *arg) { return tty_input_room(arg); }

/* Typing: each byte goes through the slave's line discipline. */
static long master_write(file_t *file, const void *buf, size_t len) {
  pty_t *pty = file->priv;
  const char *p = buf;
  size_t done = 0;
  while (done < len) {
    if (!tty_input_room(&pty->tty)) {
      if (nonblock(file))
        return done ? (long)done : -EAGAIN;
      waitqueue_wait_event(&pty->tty.read_wq, master_room, &pty->tty, 0);
    }
    tty_input_char(&pty->tty, p[done++]);
  }
  return (long)done;
}

static short master_poll(file_t *file) {
  pty_t *pty = file->priv;
  short events = 0;
  if (master_readable(pty))
    events |= FILE_POLLIN;
  if (pty->slave_opened && !pty->slave_count)
    events |= FILE_POLLHUP;
  if (tty_input_room(&pty->tty))
    events |= FILE_POLLOUT;
  return events;
}

static struct wait_queue *pty_wait_queue(file_t *file) {
  return (struct wait_queue *)&((pty_t *)file->priv)->tty.read_wq;
}

static long master_ioctl(file_t *file, unsigned long request, void *arg) {
  pty_t *pty = file->priv;
  switch (request) {
  case TIOCGPTN:
    if (!arg)
      return -EFAULT;
    *(unsigned int *)arg = (unsigned int)pty->index;
    return 0;
  case TIOCSPTLCK:
    if (!arg)
      return -EFAULT;
    spinlock_acquire(&ptys_lock);
    pty->locked = *(int *)arg != 0;
    spinlock_release(&ptys_lock);
    return 0;
  case TIOCGPTLCK:
    if (!arg)
      return -EFAULT;
    *(int *)arg = pty->locked;
    return 0;
  default: /* termios, window size: the master sets the slave's */
    return tty_ioctl(&pty->tty, request, arg);
  }
}

static void pty_stat(file_t *file, uint32_t *type, uint64_t *size) {
  (void)file;
  *type = FILE_S_IFCHR;
  *size = 0;
}

static void master_release(file_t *file) {
  pty_t *pty = file->priv;
  spinlock_acquire(&ptys_lock);
  pty->master_open = false;
  /* Hang up the slave: reads see end of file, writes fail. */
  pty->tty.hung_up = true;
  stream_close(&pty->out, true);
  waitqueue_wake_all(&pty->tty.read_wq);
  pty_t *dead = pty_unused(pty);
  spinlock_release(&ptys_lock);
  pty_free(dead);
}

static const file_ops_t master_ops = {
    .read = master_read,
    .write = master_write,
    .poll = master_poll,
    .wait_queue = pty_wait_queue,
    .ioctl = master_ioctl,
    .stat = pty_stat,
    .release = master_release,
};

/* -- Slave file -------------------------------------------- */

static long slave_read(file_t *file, void *buf, size_t len) {
  pty_t *pty = file->priv;
  if (!len)
    return 0;
  if (!tty_readable(&pty->tty) && nonblock(file))
    return -EAGAIN;
  return (long)tty_read(&pty->tty, buf, len);
}

static long slave_write(file_t *file, const void *buf, size_t len) {
  pty_t *pty = file->priv;
  if (pty->tty.hung_up)
    return -EIO;
  if (nonblock(file) && !(stream_write_poll(&pty->out) & FILE_POLLOUT))
    return -EAGAIN;
  return (long)tty_write(&pty->tty, buf, len);
}

static short slave_poll(file_t *file) {
  pty_t *pty = file->priv;
  if (pty->tty.hung_up)
    return FILE_POLLIN | FILE_POLLHUP;
  return (tty_readable(&pty->tty) ? FILE_POLLIN : 0) |
         (stream_write_poll(&pty->out) & FILE_POLLOUT);
}

static long slave_ioctl(file_t *file, unsigned long request, void *arg) {
  return tty_ioctl(&((pty_t *)file->priv)->tty, request, arg);
}

static void slave_release(file_t *file) {
  pty_t *pty = file->priv;
  spinlock_acquire(&ptys_lock);
  if (--pty->slave_count == 0) {
    /* Nobody's terminal any more; the master reads EIO. */
    pty->tty.session = pty->tty.pgrp = 0;
    waitqueue_wake_all(&pty->tty.read_wq);
  }
  pty_t *dead = pty_unused(pty);
  spinlock_release(&ptys_lock);
  pty_free(dead);
}

static const file_ops_t slave_ops = {
    .read = slave_read,
    .write = slave_write,
    .poll = slave_poll,
    .wait_queue = pty_wait_queue,
    .ioctl = slave_ioctl,
    .stat = pty_stat,
    .release = slave_release,
};

/* -- Opening ----------------------------------------------- */

/* /dev/ptmx: a new pty, opened as its master. */
static long ptmx_open(VFS_device_reg *dev, int flags, file_t **out) {
  (void)dev;
  pty_t *pty = kzalloc(sizeof(*pty));
  if (!pty)
    return -ENOMEM;
  tty_init(&pty->tty, &pty_console_ops);
  pty->tty.priv = pty;
  if (stream_init(&pty->out, PTY_OUT_SIZE, &pty->tty.read_wq) < 0) {
    kfree(pty);
    return -ENOMEM;
  }
  pty->master_open = true;
  pty->locked = true; /* until unlockpt() */

  file_t *file = file_alloc(&master_ops, file_flags(flags), pty);
  if (!file) {
    pty_free(pty);
    return -ENOMEM;
  }

  spinlock_acquire(&ptys_lock);
  int i = 0;
  while (i < PTY_MAX && ptys[i])
    i++;
  if (i < PTY_MAX) {
    pty->index = i;
    ptys[i] = pty;
  }
  spinlock_release(&ptys_lock);
  if (i == PTY_MAX) {
    kfree(file); /* never published: no release */
    pty_free(pty);
    return -ENOSPC;
  }
  *out = file;
  return 0;
}

/* /dev/pts/N: the slave of pty N, if its master unlocked it. */
static long pts_open(VFS_device_reg *dev, int flags, file_t **out) {
  int index = (int)(uintptr_t)dev->priv;
  file_t *file = file_alloc(&slave_ops, file_flags(flags), NULL);
  if (!file)
    return -ENOMEM;

  long err = 0;
  spinlock_acquire(&ptys_lock);
  pty_t *pty = ptys[index];
  if (!pty || !pty->master_open)
    err = -EIO;
  else if (pty->locked)
    err = -EIO;
  else {
    pty->slave_count++;
    pty->slave_opened = true;
    file->priv = pty;
  }
  spinlock_release(&ptys_lock);

  if (err) {
    kfree(file);
    return err;
  }
  /* A session leader without a terminal gets this one (unless O_NOCTTY),
   * as on Linux. */
  if (!(flags & O_NOCTTY))
    tty_set_ctty(&pty->tty, false);
  *out = file;
  return 0;
}

static int pty_init(void) {
  if (!dev_vfs_register_file("ptmx", ptmx_open, NULL))
    return -1;
  /* The slaves' names exist up front: registrations are never freed. */
  for (int i = 0; i < PTY_MAX; i++) {
    char name[16] = "pts/";
    if (i >= 10)
      name[4] = '0' + i / 10, name[5] = '0' + i % 10;
    else
      name[4] = '0' + i;
    dev_vfs_register_file(name, pts_open, (void *)(uintptr_t)i);
  }
  return 0;
}

module_init(pty_init);
