#include "syscall_entry.h"
#include <hubble/syscalls.h>
#include <dev/io/output_device.h>

#include <hubble/printk.h>

long sys_lseek(int fd, uint64_t offset, int whence)
{

	task_t *task = get_current_task();

	fd_entry_t *fd_entry = task_get_fd(task, fd);
	VFS_File *file = fd_entry->data;
	if (!file)
		return -1;

	return (long)vfs_lseek(file, offset, whence);

	// output_device_t *dev = get_stdout_device();
	// if (!dev || !dev->write)
	// 	return -1;

	// dev->write(buffer, len, dev->user_data);
	// return len;
}
