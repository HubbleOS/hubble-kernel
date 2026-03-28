#pragma once
#include <stdint.h>

struct udp_hdr
{
	uint16_t src_port;
	uint16_t dst_port;
	uint16_t length;
	uint16_t checksum;
} __attribute__((packed));

void udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
	      const void *data, uint16_t len);

int udp_recv(uint16_t port, void *buf_out, uint16_t *len_out);
