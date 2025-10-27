#include <sys/syscall.h>
#include <sys/syscall_nums.h>
#include <sys/input_device.h>

long sys_read(int fd, char *buffer, size_t len)
{
	if (fd != 0) // only stdin
		return -1;

	if (!buffer || len == 0)
		return -1;

	input_device_t *dev = get_stdin_device();
	if (!dev || !dev->read)
		return -1;

	size_t read_count = dev->read(buffer, len, dev->user_data);
	return (long)read_count;
}
