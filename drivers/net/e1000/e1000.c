/**
 * @file e1000.c
 * @brief Intel e1000 network adapter driver — MMIO rings, PCI probe, netdev
 */
#include <stdint.h>
#include <higher_half.h>
#include <hubble/device.h>
#include <hubble/module.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <mm/vmm.h>
#include <drivers/pci/pci.h>
#include <net/netdev.h>
#include "e1000.h"

/* ── MMIO access ────────────────────────────────────────── */

static volatile uint32_t *e1000_base = NULL;

static inline uint32_t e1000_read(uint32_t reg) { return e1000_base[reg / 4]; }
static inline void e1000_write(uint32_t reg, uint32_t val) { e1000_base[reg / 4] = val; }

/* ── Page-table helpers ─────────────────────────────────── */

static bool e1000_mmio_is_mapped(uint64_t virt)
{
	uint64_t *pml4 = pml4_table();
	if (!(pml4[PML4_INDEX(virt)] & PTE_PRESENT))
		return false;

	uint64_t *pdpt = pdpt_table(virt);
	if (!(pdpt[PDPT_INDEX(virt)] & PTE_PRESENT))
		return false;

	uint64_t *pd = pd_table(virt);
	if (!(pd[PD_INDEX(virt)] & PTE_PRESENT))
		return false;

	if (pd[PD_INDEX(virt)] & PTE_HUGE)
		return true;

	uint64_t *pt = pt_table(virt);
	return pt[PT_INDEX(virt)] & PTE_PRESENT;
}

static int e1000_map_mmio_range(uint64_t phys, uint64_t size)
{
	uint64_t start = phys & ~0xFFFULL;
	uint64_t end = (phys + size + 0xFFFULL) & ~0xFFFULL;

	for (uint64_t page = start; page < end; page += 0x1000)
	{
		uint64_t virt = phys_to_virt(page);

		if (e1000_mmio_is_mapped(virt))
			continue;

		if (vmm_map_page(virt, page, VMM_MAP_NO_CACHE) < 0)
		{
			printk(KERN_ERR "[e1000] failed to map MMIO page phys=%llx virt=%p\n",
			       page, (void *)virt);
			return -1;
		}
	}

	return 0;
}

/* ── Global state ───────────────────────────────────────── */

static struct e1000_rx_desc *rx_descs = NULL;
static struct e1000_tx_desc *tx_descs = NULL;
static uint8_t *rx_buffers[E1000_RX_DESC_COUNT];
static uint32_t rx_tail = 0;
static uint32_t tx_tail = 0;
static uint8_t mac_addr[6];

static uint8_t e1000_pci_bus, e1000_pci_slot, e1000_pci_func;

/* ── RX init ────────────────────────────────────────────── */

static void e1000_rx_init(void)
{
	rx_descs = kmalloc(sizeof(struct e1000_rx_desc) * E1000_RX_DESC_COUNT + 16, GFP_KERNEL);
	rx_descs = (struct e1000_rx_desc *)(((uint64_t)rx_descs + 15) & ~15ULL);

	memset(rx_descs, 0, sizeof(struct e1000_rx_desc) * E1000_RX_DESC_COUNT);

	for (int i = 0; i < E1000_RX_DESC_COUNT; i++)
	{
		rx_buffers[i] = kmalloc(E1000_BUFFER_SIZE, GFP_KERNEL);
		rx_descs[i].addr = virt_to_phys((uint64_t)rx_buffers[i]);
		rx_descs[i].status = 0;
	}

	uint64_t phys = virt_to_phys((uint64_t)rx_descs);
	e1000_write(E1000_RDBAL, (uint32_t)(phys & 0xFFFFFFFF));
	e1000_write(E1000_RDBAH, (uint32_t)(phys >> 32));
	e1000_write(E1000_RDLEN, E1000_RX_DESC_COUNT * sizeof(struct e1000_rx_desc));
	e1000_write(E1000_RDH, 0);
	e1000_write(E1000_RDT, E1000_RX_DESC_COUNT - 1);
	rx_tail = 0;

	e1000_write(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE_2048);
}

/* ── TX init ────────────────────────────────────────────── */

static void e1000_tx_init(void)
{
	tx_descs = kmalloc(sizeof(struct e1000_tx_desc) * E1000_TX_DESC_COUNT + 16, GFP_KERNEL);
	tx_descs = (struct e1000_tx_desc *)(((uint64_t)tx_descs + 15) & ~15ULL);

	memset(tx_descs, 0, sizeof(struct e1000_tx_desc) * E1000_TX_DESC_COUNT);

	uint64_t phys = virt_to_phys((uint64_t)tx_descs);
	e1000_write(E1000_TDBAL, (uint32_t)(phys & 0xFFFFFFFF));
	e1000_write(E1000_TDBAH, (uint32_t)(phys >> 32));
	e1000_write(E1000_TDLEN, E1000_TX_DESC_COUNT * sizeof(struct e1000_tx_desc));
	e1000_write(E1000_TDH, 0);
	e1000_write(E1000_TDT, 0);
	tx_tail = 0;

	e1000_write(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT | E1000_TCTL_COLD);
}

/* ── MAC address ────────────────────────────────────────── */

static void e1000_read_mac(void)
{
	uint32_t low = e1000_read(E1000_RAL0);
	uint32_t high = e1000_read(E1000_RAH0);

	mac_addr[0] = (low >> 0) & 0xFF;
	mac_addr[1] = (low >> 8) & 0xFF;
	mac_addr[2] = (low >> 16) & 0xFF;
	mac_addr[3] = (low >> 24) & 0xFF;
	mac_addr[4] = (high >> 0) & 0xFF;
	mac_addr[5] = (high >> 8) & 0xFF;

	printk(KERN_INFO "[e1000] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
	       mac_addr[0], mac_addr[1], mac_addr[2],
	       mac_addr[3], mac_addr[4], mac_addr[5]);
}

void e1000_get_mac(uint8_t out[6])
{
	for (int i = 0; i < 6; i++)
		out[i] = mac_addr[i];
}

/* ── Transmit ───────────────────────────────────────────── */

int e1000_send(const void *data, uint16_t len)
{
	uint32_t idx = tx_tail % E1000_TX_DESC_COUNT;

	uint64_t phys = virt_to_phys((uint64_t)data);
	printk(KERN_INFO "[tx] idx=%d phys=%llx len=%d\n", idx, phys, len);
	printk(KERN_INFO "[tx] TDH=%d TDT=%d\n", e1000_read(E1000_TDH), e1000_read(E1000_TDT));
	printk(KERN_INFO "[tx] STATUS before=%02x\n", tx_descs[idx].status);

	tx_descs[idx].addr = phys;
	tx_descs[idx].length = len;
	tx_descs[idx].cmd = E1000_TX_CMD_EOP | E1000_TX_CMD_RS;
	tx_descs[idx].status = 0;

	tx_tail = (tx_tail + 1) % E1000_TX_DESC_COUNT;
	e1000_write(E1000_TDT, tx_tail);

	printk(KERN_INFO "[tx] TDT written=%d\n", tx_tail);
	printk(KERN_INFO "[tx] TCTL=%08x TDBAL=%08x TDBAH=%08x TDLEN=%08x\n",
	       e1000_read(E1000_TCTL),
	       e1000_read(E1000_TDBAL),
	       e1000_read(E1000_TDBAH),
	       e1000_read(E1000_TDLEN));

	for (volatile int i = 0; i < 10000000; i++)
		;
	printk(KERN_INFO "[tx] STATUS after wait=%02x\n", tx_descs[idx].status);

	while (!(tx_descs[idx].status & E1000_TX_STAT_DD))
		;
	return 0;
}

/* ── Receive ────────────────────────────────────────────── */

int e1000_recv(void *buf, uint16_t *len_out)
{
	uint32_t idx = rx_tail % E1000_RX_DESC_COUNT;

	static int first = 0;
	if (!first)
	{
		first = 1;
		printk(KERN_INFO "[rx] FIRST CALL: idx=%d status=%02x RDH=%d RDT=%d RDBAL=%08x\n",
		       idx,
		       rx_descs[idx].status,
		       e1000_read(E1000_RDH),
		       e1000_read(E1000_RDT),
		       e1000_read(E1000_RDBAL));
	}

	uint8_t st = rx_descs[idx].status;
	if (st != 0)
		printk(KERN_INFO "[rx] idx=%d status=%02x RDH=%d\n",
		       idx, st, e1000_read(E1000_RDH));

	if (!(st & E1000_RX_STAT_DD))
		return -1;

	uint16_t len = rx_descs[idx].length;
	memcpy(buf, rx_buffers[idx], len);
	*len_out = len;

	rx_descs[idx].status = 0;

	e1000_write(E1000_RDT, idx);
	rx_tail = (rx_tail + 1) % E1000_RX_DESC_COUNT;

	return 0;
}

/* ── PCI probe ──────────────────────────────────────────── */

static int e1000_probe(struct pci_device *pci_dev)
{
	e1000_pci_bus = pci_dev->bus;
	e1000_pci_slot = pci_dev->slot;
	e1000_pci_func = pci_dev->func;

	uint64_t bar0 = pci_dev->bar0;
	if (!bar0)
	{
		printk(KERN_ERR "[e1000] bad BAR0\n");
		return -1;
	}

	uint32_t pci_cmd = pci_read_config(e1000_pci_bus, e1000_pci_slot, e1000_pci_func, 0x04);
	pci_cmd |= (1 << 2) | (1 << 1);
	pci_write_config(e1000_pci_bus, e1000_pci_slot, e1000_pci_func, 0x04, pci_cmd);

	if (e1000_map_mmio_range(bar0, E1000_MMIO_SIZE) < 0)
		return -1;

	e1000_base = (volatile uint32_t *)phys_to_virt(bar0);

	printk(KERN_INFO "[e1000] bar0 phys=%llx virt=%p\n", bar0, e1000_base);

	e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_RST);
	for (volatile int i = 0; i < 1000000; i++)
		;
	e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_SLU);

	e1000_read_mac();
	e1000_tx_init();
	e1000_rx_init();

	static struct netdev_ops ops = {
	    .send = e1000_send,
	    .recv = e1000_recv,
	};
	static struct netdev_data data;
	static struct device dev = {
	    .name = "eth0",
	    .type = DEV_NET,
	    .ops = &ops,
	    .priv = &data,
	};

	device_register(&dev);

	e1000_get_mac(data.mac);

	printk(KERN_OK "[e1000] init OK\n");
	return 0;
}

/* ── Driver registration ────────────────────────────────── */

static const struct pci_device_id e1000_ids[] = {
    {E1000_VENDOR_ID, E1000_DEVICE_ID},
    {0, 0},
};

static struct pci_driver e1000_driver = {
    .name = "e1000",
    .id_table = e1000_ids,
    .probe = e1000_probe,
};

__init int e1000_module_init(void)
{
	printk(KERN_INFO "[e1000] module init\n");
	return pci_register_driver(&e1000_driver);
}

module_init(e1000_module_init);
MODULE_NAME("e1000");
