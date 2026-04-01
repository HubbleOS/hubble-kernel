#include "e1000.h"
#include <hubble/device.h>
#include <hubble/init.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <mm/vmm.h>
#include <drivers/pci/pci.h>
#include <higher_half.h>
#include <net/netdev.h>

static volatile uint32_t *e1000_base = NULL;

static inline uint32_t e1000_read(uint32_t reg) { return e1000_base[reg / 4]; }
static inline void e1000_write(uint32_t reg, uint32_t val) { e1000_base[reg / 4] = val; }

static struct e1000_rx_desc *rx_descs = NULL;
static struct e1000_tx_desc *tx_descs = NULL;
static uint8_t *rx_buffers[E1000_RX_DESC_COUNT];
static uint32_t rx_tail = 0;
static uint32_t tx_tail = 0;
static uint8_t mac_addr[6];

// bus/slot/func теперь приходят снаружи через pci_device
static uint8_t e1000_pci_bus, e1000_pci_slot, e1000_pci_func;

// PCI find e1000
// Global coordinates of the PCI device
static uint8_t e1000_pci_bus, e1000_pci_slot, e1000_pci_func;

static void e1000_rx_init(void)
{
	rx_descs = kmalloc(sizeof(struct e1000_rx_desc) * E1000_RX_DESC_COUNT + 16, GFP_KERNEL);

	// Align manually
	rx_descs = (struct e1000_rx_desc *)(((uint64_t)rx_descs + 15) & ~15ULL);

	memset(rx_descs, 0, sizeof(struct e1000_rx_desc) * E1000_RX_DESC_COUNT);

	for (int i = 0; i < E1000_RX_DESC_COUNT; i++)
	{
		rx_buffers[i] = kmalloc(E1000_BUFFER_SIZE, GFP_KERNEL);
		rx_descs[i].addr = VIRT_TO_PHYS(rx_buffers[i]);
		rx_descs[i].status = 0;
	}

	uint64_t phys = VIRT_TO_PHYS(rx_descs);
	e1000_write(E1000_RDBAL, (uint32_t)(phys & 0xFFFFFFFF));
	e1000_write(E1000_RDBAH, (uint32_t)(phys >> 32));
	e1000_write(E1000_RDLEN, E1000_RX_DESC_COUNT * sizeof(struct e1000_rx_desc));
	e1000_write(E1000_RDH, 0);
	e1000_write(E1000_RDT, E1000_RX_DESC_COUNT - 1);
	rx_tail = 0;

	e1000_write(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE_2048);
}

static void e1000_tx_init(void)
{
	tx_descs = kmalloc(sizeof(struct e1000_tx_desc) * E1000_TX_DESC_COUNT + 16, GFP_KERNEL);
	// Align manually
	tx_descs = (struct e1000_tx_desc *)(((uint64_t)tx_descs + 15) & ~15ULL);

	memset(tx_descs, 0, sizeof(struct e1000_tx_desc) * E1000_TX_DESC_COUNT);

	uint64_t phys = VIRT_TO_PHYS(tx_descs);
	e1000_write(E1000_TDBAL, (uint32_t)(phys & 0xFFFFFFFF));
	e1000_write(E1000_TDBAH, (uint32_t)(phys >> 32));
	e1000_write(E1000_TDLEN, E1000_TX_DESC_COUNT * sizeof(struct e1000_tx_desc));
	e1000_write(E1000_TDH, 0);
	e1000_write(E1000_TDT, 0);
	tx_tail = 0;

	e1000_write(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT | E1000_TCTL_COLD);
}

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

	printk("[e1000] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
	       mac_addr[0], mac_addr[1], mac_addr[2],
	       mac_addr[3], mac_addr[4], mac_addr[5]);
}

void e1000_get_mac(uint8_t out[6])
{
	for (int i = 0; i < 6; i++)
		out[i] = mac_addr[i];
}

int e1000_send(const void *data, uint16_t len)
{
	uint32_t idx = tx_tail % E1000_TX_DESC_COUNT;

	uint64_t phys = VIRT_TO_PHYS(data);
	printk("[tx] idx=%d phys=%llx len=%d\n", idx, phys, len);
	printk("[tx] TDH=%d TDT=%d\n", e1000_read(E1000_TDH), e1000_read(E1000_TDT));
	printk("[tx] STATUS before=%02x\n", tx_descs[idx].status);

	tx_descs[idx].addr = phys;
	tx_descs[idx].length = len;
	tx_descs[idx].cmd = E1000_TX_CMD_EOP | E1000_TX_CMD_RS;
	tx_descs[idx].status = 0;

	tx_tail = (tx_tail + 1) % E1000_TX_DESC_COUNT;
	e1000_write(E1000_TDT, tx_tail);

	printk("[tx] TDT written=%d\n", tx_tail);
	printk("[tx] TCTL=%08x TDBAL=%08x TDBAH=%08x TDLEN=%08x\n",
	       e1000_read(E1000_TCTL),
	       e1000_read(E1000_TDBAL),
	       e1000_read(E1000_TDBAH),
	       e1000_read(E1000_TDLEN));

	for (volatile int i = 0; i < 10000000; i++)
		;
	printk("[tx] STATUS after wait=%02x\n", tx_descs[idx].status);

	while (!(tx_descs[idx].status & E1000_TX_STAT_DD))
		;
	return 0;
}

int e1000_recv(void *buf, uint16_t *len_out)
{
	uint32_t idx = rx_tail % E1000_RX_DESC_COUNT;

	static int first = 0;
	if (!first)
	{
		first = 1;
		printk("[rx] FIRST CALL: idx=%d status=%02x RDH=%d RDT=%d RDBAL=%08x\n",
		       idx,
		       rx_descs[idx].status,
		       e1000_read(E1000_RDH),
		       e1000_read(E1000_RDT),
		       e1000_read(E1000_RDBAL));
	}

	uint8_t st = rx_descs[idx].status;
	if (st != 0)
		printk("[rx] idx=%d status=%02x RDH=%d\n",
		       idx, st, e1000_read(E1000_RDH));

	if (!(st & E1000_RX_STAT_DD))
		return -1;

	uint16_t len = rx_descs[idx].length;
	memcpy(buf, rx_buffers[idx], len);
	*len_out = len;

	rx_descs[idx].status = 0;

	// We return the NIC descriptor - we write the CURRENT idx, not the next one
	e1000_write(E1000_RDT, idx);
	rx_tail = (rx_tail + 1) % E1000_RX_DESC_COUNT;

	return 0;
}

#define E1000_MMIO_SIZE 0x20000

static int e1000_probe(struct pci_device *pci_dev)
{
	e1000_pci_bus = pci_dev->bus;
	e1000_pci_slot = pci_dev->slot;
	e1000_pci_func = pci_dev->func;

	uint64_t bar0 = pci_dev->bar0;
	if (!bar0)
	{
		printk("[e1000] bad BAR0\n");
		return -1;
	}

	// Enable PCI Bus Mastering + Memory Space
	uint32_t pci_cmd = pci_read_config(e1000_pci_bus, e1000_pci_slot, e1000_pci_func, 0x04);
	pci_cmd |= (1 << 2) | (1 << 1);
	pci_write_config(e1000_pci_bus, e1000_pci_slot, e1000_pci_func, 0x04, pci_cmd);

	uint64_t mmio_virt = (uint64_t)PHYS_TO_VIRT_MMIO(bar0);
	for (uint64_t off = 0; off < E1000_MMIO_SIZE; off += 0x1000)
		vmm_map_page(mmio_virt + off, bar0 + off,
			     PTE_PRESENT | PTE_WRITE | VMM_MAP_NO_CACHE);

	e1000_base = (volatile uint32_t *)mmio_virt;

	printk("[e1000] bar0 phys=%llx virt=%p\n", bar0, e1000_base);

	e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_RST);
	for (volatile int i = 0; i < 1000000; i++)
		;
	e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_SLU);

	e1000_read_mac();
	e1000_tx_init();
	e1000_rx_init();

	// register netdev
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
	e1000_get_mac(data.mac);
	device_register(&dev);

	printk("[e1000] init OK\n");
	return 0;
}

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
	printk("[e1000] module init\n");
	return pci_register_driver(&e1000_driver);
}

device_initcall(e1000_module_init);
