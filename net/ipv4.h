#pragma once

#include <stdint.h>

#define IP_PROTO_UDP 17

struct ip_hdr
{
	uint8_t ihl_ver; // version=4, ihl=5
	uint8_t tos;
	uint16_t tot_len;
	uint16_t id;
	uint16_t frag_off;
	uint8_t ttl;
	uint8_t proto;
	uint16_t checksum;
	uint32_t src;
	uint32_t dst;
} __attribute__((packed));

uint16_t ip_checksum(const void *data, uint16_t len);
void ip_send(uint32_t dst_ip, uint8_t proto, const void *payload, uint16_t len);
