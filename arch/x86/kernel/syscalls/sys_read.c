#include "syscall_entry.h"
#include <hubble/syscalls.h>

#include <hubble/printk.h>

long sys_read(int fd, char *buffer, size_t len)
{
	if (fd < 0 || fd >= MAX_FDS || !buffer || len == 0)
	{
		return -1;
	}
	task_t *current = get_current_task();
	VFS_File *file = task_get_fd(current, fd);

	size_t read_count = vfs_read(file, buffer, len);
	return (long)read_count;
}
