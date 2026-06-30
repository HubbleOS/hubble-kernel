/**
 * @file netdev.h
 * @brief Network device interface
 */

#pragma once

#include <stdint.h>

/**
 * @brief Network device operations (send/receive)
 */
struct netdev_ops
{
	int (*send)(const void *data, uint16_t len);
	int (*recv)(void *buf, uint16_t *len);
};

/**
 * @brief Network device private data
 */
struct netdev_data
{
	uint8_t mac[6];
};
