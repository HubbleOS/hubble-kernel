#include <drivers/tty/tty.h>
#include <fs/vfs/dev.h>
#include <hubble/module.h>
#include <hubble/string.h>

/* ── tty instance ────────────────────────────────────────────────────────── */

static tty_t tty0;

/* ── Console ops ─────────────────────────────────────────────────────────── */

/* early_putchar з textout.c — вже обробляє \n, \b, scroll */
extern void early_putchar(char c);

static const tty_console_ops_t tty0_console_ops = {
    .putchar = early_putchar,
    .clear = NULL, /* TODO: реалізувати clear screen якщо потрібно */
};

/* ── VFS callbacks ───────────────────────────────────────────────────────── */

static uint64_t tty_vfs_read(uint64_t offset, size_t size, void *buf)
{
	(void)offset;
	size_t n = tty_read(&tty0, (char *)buf, size);
	return n < 0 ? 0 : (uint64_t)n;
}

static uint64_t tty_vfs_write(uint64_t offset, size_t size, const void *buf)
{
	(void)offset;
	size_t n = tty_write(&tty0, (const char *)buf, size);
	return n < 0 ? 0 : (uint64_t)n;
}

/* ── initcall ────────────────────────────────────────────────────────────── */

static int tty_dev_init(void)
{
	tty_init(&tty0, &tty0_console_ops);
	dev_vfs_register("tty0", NULL, tty_vfs_read, tty_vfs_write);
	return 0;
}

module_init(tty_dev_init);
MODULE_NAME("tty");
