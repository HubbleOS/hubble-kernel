#include "netdev.h"

#include <stddef.h>

static struct netdev *current_dev = NULL;

void netdev_register(struct netdev *dev)
{
	current_dev = dev;
}

struct netdev *netdev_get(void)
{
	return current_dev;
}