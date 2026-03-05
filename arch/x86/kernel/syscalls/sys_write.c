#include "syscall_entry.h"
#include <dev/io/output_device.h>

#include "printk.h"

long sys_write(int fd, const char *buffer, size_t len)
{
	if (fd != 1) // only stdout
		return -1;

	if (!buffer)
		return -1;

	printk("%s", buffer);

	output_device_t *dev = get_stdout_device();
	if (!dev || !dev->write)
		return -1;

	dev->write(buffer, len, dev->user_data);
	return len;
}
