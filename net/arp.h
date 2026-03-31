#pragma once
#include <stdint.h>

#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IP 0x0800

#define ARP_REQUEST 1
#define ARP_REPLY 2

struct arp_pkt
{
	uint16_t htype; // hardware type = 1 (ethernet)
	uint16_t ptype; // protocol type = 0x0800 (IP)
	uint8_t hlen;	// hardware addr len = 6
	uint8_t plen;	// protocol addr len = 4
	uint16_t oper;	// 1=request, 2=reply
	uint8_t sha[6]; // sender MAC
	uint32_t spa;	// sender IP
	uint8_t tha[6]; // target MAC
	uint32_t tpa;	// target IP
} __attribute__((packed));

// IP format 10.0.2.15 -> arp_ip(10,0,2,15)
#define ARP_IP(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

int arp_request(uint32_t target_ip);
int arp_lookup(uint32_t ip, uint8_t mac_out[6]);
void arp_handle(const uint8_t *pkt, uint16_t len);
