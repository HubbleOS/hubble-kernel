#include "eth.h"
#include "netdev.h"
#include <net/arp.h>
#include <mm/kmalloc.h>
#include <hubble/string.h>

struct eth_hdr
{
	uint8_t dst[6];
	uint8_t src[6];
	uint16_t type;
} __attribute__((packed));

void eth_send(uint8_t dst[6], uint16_t type, const void *payload, uint16_t len)
{
	struct netdev *dev = netdev_get();
	uint8_t *buf = kmalloc(1518, GFP_KERNEL);

	struct eth_hdr *hdr = (struct eth_hdr *)buf;
	memcpy(hdr->dst, dst, 6);
	memcpy(hdr->src, dev->mac, 6);
	hdr->type = __builtin_bswap16(type);
	memcpy(buf + sizeof(struct eth_hdr), payload, len);

	uint16_t total = sizeof(struct eth_hdr) + len;
	if (total < 64)
		total = 64;

	dev->send(buf, total);
	kfree(buf);
}

int eth_recv(uint8_t *payload_out, uint16_t *len_out, uint16_t *type_out)
{
	struct netdev *dev = netdev_get();
	uint8_t buf[2048];
	uint16_t len;

	if (dev->recv(buf, &len) != 0)
		return -1;

	struct eth_hdr *hdr = (struct eth_hdr *)buf;
	uint16_t type = __builtin_bswap16(hdr->type);
	uint8_t *payload = buf + sizeof(struct eth_hdr);
	uint16_t payload_len = len - sizeof(struct eth_hdr);

	if (type == ETH_TYPE_ARP)
	{
		arp_handle(payload, payload_len);
		return -1;
	}

	if (type == ETH_TYPE_IP)
	{
		if (type_out)
			*type_out = type;
		memcpy(payload_out, payload, payload_len);
		*len_out = payload_len;
		return 0;
	}

	return -1;
}
