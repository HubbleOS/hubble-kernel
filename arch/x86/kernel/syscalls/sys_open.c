#include <smp/scheduler.h>
#include <sys/syscall.h>
#include <smp/task.h>
#include <fs/vfs/dev.h>
#include <fs/vfs/vfs.h>
#include "higher_half.h"
#include <string.h>

#include "syscall_entry.h"

// sys_open - returns fd integer
long sys_open(const char *path, int flags)
{
	task_t *current = get_current_task();

	VFS_File *file = vfs_open(path, flags);
	if (!file)
		return -1;

	// Find free fd slot
	for (int i = 0; i < MAX_FDS; i++)
	{
		if (!current->fds[i])
		{
			current->fds[i] = file;
			return i; // return fd number
		}
	}
	vfs_close(file);
	return -1; // too many open files
}

// sys_close
long sys_close(int fd)
{
	task_t *current = get_current_task();
	if (fd < 0 || fd >= MAX_FDS || !current->fds[fd])
		return -1;
	vfs_close(current->fds[fd]);
	current->fds[fd] = NULL;
	return 0;
}

// kernel:
// long sys_read_file(int fd, void *buf, size_t size)
// {
// 	task_t *current = get_current_task();
// 	VFS_File *file = task_get_fd(current, fd);
// 	if (!file)
// 		return -1;

// 	VFS_device_reg *dev = (VFS_device_reg *)file->node->fs_node;
// 	if (!dev || !dev->read)
// 		return -1;

// 	// dev->read returns physical address of data
// 	uint64_t phys = dev->read(0, size,);
// 	void *src = PHYS_TO_VIRT_PTR(void, phys);

// 	// copy to userspace buffer
// 	memcpy(buf, src, size);
// 	return size;
// }

// lookup helper used by sys_mmap
