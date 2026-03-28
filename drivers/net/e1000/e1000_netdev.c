#include "e1000.h"
#include <net/netdev.h>
#include <string.h>

static void e1000_netdev_send(const void *data, uint16_t len)
{
	e1000_send(data, len);
}

static int e1000_netdev_recv(void *buf, uint16_t *len)
{
	return e1000_recv(buf, len);
}

void e1000_netdev_register(void)
{
	static struct netdev dev;

	strncpy(dev.name, "eth0", 16);
	e1000_get_mac(dev.mac);
	dev.send = e1000_netdev_send;
	dev.recv = e1000_netdev_recv;

	netdev_register(&dev);
}
