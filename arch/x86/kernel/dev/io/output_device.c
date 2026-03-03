#include <sys/output_device.h>

static output_device_t *stdout_dev = NULL;

void register_stdout_device(output_device_t *dev)
{
	stdout_dev = dev;
}

output_device_t *get_stdout_device(void)
{
	return stdout_dev;
}
