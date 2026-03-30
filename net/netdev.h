#pragma once
#include <stdint.h>

struct netdev_ops
{
	int (*send)(const void *data, uint16_t len);
	int (*recv)(void *buf, uint16_t *len);
};

struct netdev_data
{
	uint8_t mac[6];
};
