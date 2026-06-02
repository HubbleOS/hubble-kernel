#include <hubble/device.h>
#include <hubble/string.h>
#include <hubble/printk.h>

#define MAX_DEVICES 32
static struct device *devices[MAX_DEVICES];
static int device_count = 0;

void device_register(struct device *dev)
{
	if (device_count >= MAX_DEVICES)
	{
		printk(KERN_ERR "[device] too many devices, dropping %s\n", dev->name);
		return;
	}
	devices[device_count++] = dev;
	printk(KERN_INFO "[device] registered: %s (type=%u)\n", dev->name, dev->type);
}

struct device *device_find_by_name(const char *name)
{
	for (int i = 0; i < device_count; i++)
		if (strcmp(devices[i]->name, name) == 0)
			return devices[i];
	return NULL;
}

struct device *device_find_by_type(uint32_t type)
{
	for (int i = 0; i < device_count; i++)
		if (devices[i]->type == type)
			return devices[i];
	return NULL;
}
