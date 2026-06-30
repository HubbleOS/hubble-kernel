/**
 * @file arp.c
 * @brief ARP (Address Resolution Protocol) implementation
 *
 * Handles ARP requests, replies, and maintains a small cache mapping
 * IP addresses to MAC addresses.
 */

#include <hubble/printk.h>
#include <hubble/string.h>
#include <drivers/net/e1000/e1000.h>
#include <net/eth.h>
#include "arp.h"

#define ARP_CACHE_SIZE 16

static struct
{
	uint32_t ip;
	uint8_t mac[6];
	int valid;
} arp_cache[ARP_CACHE_SIZE];

static uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

#define MY_IP ARP_IP(10, 0, 2, 15)
#define GW_IP ARP_IP(10, 0, 2, 2)

/**
 * @brief Send an ARP request for a given IP address
 * @param target_ip Target IP address (network byte order)
 * @return 0 on success
 */
int arp_request(uint32_t target_ip)
{
	struct arp_pkt pkt;
	memset(&pkt, 0, sizeof(pkt));

	uint8_t my_mac[6];
	e1000_get_mac(my_mac);

	pkt.htype = __builtin_bswap16(1);
	pkt.ptype = __builtin_bswap16(0x0800);
	pkt.hlen = 6;
	pkt.plen = 4;
	pkt.oper = __builtin_bswap16(ARP_REQUEST);

	memcpy(pkt.sha, my_mac, 6);
	pkt.spa = MY_IP;
	memset(pkt.tha, 0, 6);
	pkt.tpa = target_ip;

	eth_send(broadcast, ETH_TYPE_ARP, &pkt, sizeof(pkt));
	printk(KERN_INFO "[arp] request sent for %d.%d.%d.%d\n",
	       ((uint8_t *)&target_ip)[0], ((uint8_t *)&target_ip)[1],
	       ((uint8_t *)&target_ip)[2], ((uint8_t *)&target_ip)[3]);
	return 0;
}

/**
 * @brief Handle an incoming ARP packet (cache replies)
 * @param pkt Pointer to the ARP packet data
 * @param len Length of the ARP packet
 */
void arp_handle(const uint8_t *pkt, uint16_t len)
{
	if (len < sizeof(struct arp_pkt))
		return;

	const struct arp_pkt *a = (const struct arp_pkt *)pkt;

	if (__builtin_bswap16(a->oper) == ARP_REPLY)
	{
		for (int i = 0; i < ARP_CACHE_SIZE; i++)
		{
			if (!arp_cache[i].valid || arp_cache[i].ip == a->spa)
			{
				arp_cache[i].ip = a->spa;
				memcpy(arp_cache[i].mac, a->sha, 6);
				arp_cache[i].valid = 1;
				printk(KERN_INFO "[arp] cached %d.%d.%d.%d -> %02x:%02x:%02x:%02x:%02x:%02x\n",
				       ((uint8_t *)&a->spa)[0], ((uint8_t *)&a->spa)[1],
				       ((uint8_t *)&a->spa)[2], ((uint8_t *)&a->spa)[3],
				       a->sha[0], a->sha[1], a->sha[2],
				       a->sha[3], a->sha[4], a->sha[5]);
				break;
			}
		}
	}
}

/**
 * @brief Look up a MAC address for an IP in the ARP cache
 * @param ip IP address to look up
 * @param mac_out Buffer for the MAC address (6 bytes)
 * @return 0 on success, -1 if not found
 */
int arp_lookup(uint32_t ip, uint8_t mac_out[6])
{
	for (int i = 0; i < ARP_CACHE_SIZE; i++)
	{
		if (arp_cache[i].valid && arp_cache[i].ip == ip)
		{
			memcpy(mac_out, arp_cache[i].mac, 6);
			return 0;
		}
	}
	return -1;
}
