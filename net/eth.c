#include "eth.h"
#include <hubble/device.h>
#include <net/netdev.h>
#include <net/arp.h>
#include <mm/kmalloc.h>
#include <hubble/string.h>

struct eth_hdr
{
	uint8_t dst[6];
	uint8_t src[6];
	uint16_t type;
} __attribute__((packed));

static struct device *eth_dev(void)
{
	return device_find_by_type(DEV_NET);
}
#include <hubble/printk.h>
void eth_send(uint8_t dst[6], uint16_t type, const void *payload, uint16_t len)
{
	struct device *dev = eth_dev();
	struct netdev_ops *ops = dev->ops;
	struct netdev_data *data = dev->priv;

	uint8_t *buf = kmalloc(1518, GFP_KERNEL);

	struct eth_hdr *hdr = (struct eth_hdr *)buf;
	memcpy(hdr->dst, dst, 6);
	memcpy(hdr->src, data->mac, 6);
	hdr->type = __builtin_bswap16(type);
	memcpy(buf + sizeof(struct eth_hdr), payload, len);

	uint16_t total = sizeof(struct eth_hdr) + len;
	if (total < 64)
		total = 64;

	ops->send(buf, total);
	kfree(buf);
}

int eth_recv(uint8_t *payload_out, uint16_t *len_out, uint16_t *type_out)
{
	struct device *dev = eth_dev();
	struct netdev_ops *ops = dev->ops;

	uint8_t buf[2048];
	uint16_t len;

	if (ops->recv(buf, &len) != 0)
		return -1;

	struct eth_hdr *hdr = (struct eth_hdr *)buf;
	uint16_t type = __builtin_bswap16(hdr->type);
	uint8_t *payload = buf + sizeof(struct eth_hdr);
	uint16_t plen = len - sizeof(struct eth_hdr);

	if (type == ETH_TYPE_ARP)
	{
		arp_handle(payload, plen);
		return -1;
	}

	if (type == ETH_TYPE_IP)
	{
		if (type_out)
			*type_out = type;
		memcpy(payload_out, payload, plen);
		*len_out = plen;
		return 0;
	}

	return -1;
}
