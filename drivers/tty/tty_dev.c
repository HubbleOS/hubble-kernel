/**
 * @file tty_dev.c
 * @brief TTY VFS device — registers tty0 as a character device
 */
#include <drivers/tty/tty.h>
#include <fs/vfs/dev.h>
#include <hubble/module.h>
#include <hubble/string.h>

/* -- TTY instance ----------------------------------------- */

static tty_t tty0;

/* -- Console ops ------------------------------------------ */

extern void printk_console_write(const char *buf, size_t len);
extern void console_clear(void);
extern void console_get_size(uint16_t *cols, uint16_t *rows);

/* Echo runs in the keyboard IRQ; going through the printk lock keeps it
 * from interleaving with output on another CPU mid escape sequence. */
static void tty0_putchar(char c) { printk_console_write(&c, 1); }

static const tty_console_ops_t tty0_console_ops = {
    .putchar = tty0_putchar,
    .clear = console_clear,
    .get_size = console_get_size,
};

/* -- VFS callbacks ---------------------------------------- */

static uint64_t tty_vfs_read(uint64_t offset, size_t size, void *buf) {
  (void)offset;
  size_t n = tty_read(&tty0, (char *)buf, size);
  return n < 0 ? 0 : (uint64_t)n;
}

static uint64_t tty_vfs_write(uint64_t offset, size_t size, const void *buf) {
  (void)offset;
  size_t n = tty_write(&tty0, (const char *)buf, size);
  return n < 0 ? 0 : (uint64_t)n;
}

static long tty_vfs_ioctl(unsigned long request, void *arg) {
  return tty_ioctl(&tty0, request, arg);
}

static bool tty_vfs_readable(void) { return tty_readable(&tty0); }

/* -- Initcall --------------------------------------------- */

static int tty_dev_init(void) {
  tty_init(&tty0, &tty0_console_ops);
  dev_vfs_register("tty0", NULL, tty_vfs_read, tty_vfs_write);
  dev_vfs_set_char_ops("tty0", tty_vfs_ioctl, tty_vfs_readable,
                       (struct wait_queue *)&tty0.read_wq);
  return 0;
}

module_init(tty_dev_init);
MODULE_NAME("tty");
