#include "ipv4.h"
#include "arp.h"
#include <hubble/string.h>
#include <printk.h>
#include <mm/kmalloc.h>

#include <net/eth.h>

#define MY_IP ARP_IP(10, 0, 2, 15)

uint16_t ip_checksum(const void *data, uint16_t len)
{
	const uint16_t *ptr = (const uint16_t *)data;
	uint32_t sum = 0;

	while (len > 1)
	{
		sum += *ptr++;
		len -= 2;
	}
	if (len)
		sum += *(uint8_t *)ptr;

	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~(uint16_t)sum;
}

void ip_send(uint32_t dst_ip, uint8_t proto, const void *payload, uint16_t payload_len)
{
	uint8_t dst_mac[6];
	if (arp_lookup(dst_ip, dst_mac) != 0)
	{
		printk("[ip] no ARP entry\n");
		return;
	}

	printk("[ip] dst_mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
		   dst_mac[0], dst_mac[1], dst_mac[2],
		   dst_mac[3], dst_mac[4], dst_mac[5]);
	printk("[ip] dst=%d.%d.%d.%d proto=%d len=%d\n",
		   ((uint8_t *)&dst_ip)[0], ((uint8_t *)&dst_ip)[1],
		   ((uint8_t *)&dst_ip)[2], ((uint8_t *)&dst_ip)[3],
		   proto, payload_len);

	uint16_t total = sizeof(struct ip_hdr) + payload_len;
	uint8_t *buf = kmalloc(total, GFP_KERNEL);
	memset(buf, 0, total);

	struct ip_hdr *hdr = (struct ip_hdr *)buf;
	hdr->ihl_ver = 0x45; // IPv4, IHL=5
	hdr->tos = 0;
	hdr->tot_len = __builtin_bswap16(total);
	hdr->id = 0;
	hdr->frag_off = 0;
	hdr->ttl = 64;
	hdr->proto = proto;
	hdr->checksum = 0;
	hdr->src = MY_IP;
	hdr->dst = dst_ip;
	hdr->checksum = ip_checksum(hdr, sizeof(struct ip_hdr));

	memcpy(buf + sizeof(struct ip_hdr), payload, payload_len);

	eth_send(dst_mac, ETH_TYPE_IP, buf, total);
	kfree(buf);
}
