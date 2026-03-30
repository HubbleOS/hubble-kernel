#pragma once
#include <stdint.h>

#define DEV_SOUND 1
#define DEV_NET 2
#define DEV_OTHER 3

struct device
{
	const char *name;
	uint32_t type; // DEV_SOUND, DEV_NET etc
	void *ops;     // pointer to ops driver interface
	void *priv;    // private driver data
};

// register/find devices
void device_register(struct device *dev);
struct device *device_find_by_name(const char *name);
struct device *device_find_by_type(uint32_t type);
