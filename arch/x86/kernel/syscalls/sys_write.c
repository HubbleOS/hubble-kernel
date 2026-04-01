#include "syscall_entry.h"
#include <hubble/syscalls.h>

#include <hubble/printk.h>

long sys_write(int fd, const char *buffer, size_t len)
{
	if (fd != 1) // only stdout
		return -1;

	if (!buffer)
		return -1;

	printk("%.*s", (int)len, buffer);

	return len;
}
