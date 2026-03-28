#pragma once
#include <stdint.h>

struct netdev
{
	char name[16];
	uint8_t mac[6];

	void (*send)(const void *data, uint16_t len);
	int (*recv)(void *buf, uint16_t *len);
};

void netdev_register(struct netdev *dev);
struct netdev *netdev_get(void);
