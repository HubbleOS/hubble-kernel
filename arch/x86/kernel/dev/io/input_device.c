#include "input_device.h"

static input_device_t *stdin_dev = NULL;

void register_stdin_device(input_device_t *dev)
{
	stdin_dev = dev;
}

input_device_t *get_stdin_device(void)
{
	return stdin_dev;
}
