/**
 * @file e1000.c
 * @brief Intel e1000 / e1000e Ethernet driver (net.ko)
 *
 * Covers the 8254x family (QEMU's default card, VirtualBox, VMware) and
 * the 82574L (QEMU's e1000e). Polled, like the xHCI driver: the network
 * stack calls poll() every few milliseconds, and interrupts stay masked.
 *
 * The rings and packet buffers are whole pages from the page allocator,
 * so the card's DMA addresses are plain physical addresses: two 2048-byte
 * buffers per page. Sending copies the frame into the next transmit
 * buffer and moves the tail; nothing waits for the card.
 */
#include "e1000.h"

#include <drivers/pci/pci.h>
#include <apic/apic.h>
#include <higher_half.h>
#include <hpet/hpet.h>
#include <hubble/module.h>
#include <hubble/netdev.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define E1000_MAX_CARDS 4

/* -- Supported cards -------------------------------------- */

enum e1000_kind {
  E1000_8254X, /* EERD: start bit 0, done bit 4, address << 8 */
  E1000_82574, /* EERD: start bit 0, done bit 1, address << 2 */
};

static const struct {
  uint16_t device;
  enum e1000_kind kind;
  const char *name;
} e1000_cards[] = {
    {0x100E, E1000_8254X, "82540EM"}, /* QEMU -nic model=e1000 */
    {0x100F, E1000_8254X, "82545EM"}, /* VMware */
    {0x1004, E1000_8254X, "82543GC"},
    {0x100C, E1000_8254X, "82544GC"},
    {0x1015, E1000_8254X, "82540EM LOM"},
    {0x1019, E1000_8254X, "82547EI"},
    {0x101E, E1000_8254X, "82540EP LP"},
    {0x1026, E1000_8254X, "82545GM"},
    {0x107C, E1000_8254X, "82541PI"},
    {0x10D3, E1000_82574, "82574L"}, /* QEMU -nic model=e1000e */
    {0x10F6, E1000_82574, "82574LA"},
};

struct e1000 {
  struct netdev netdev;
  volatile uint32_t *mmio;
  enum e1000_kind kind;

  struct e1000_rx_desc *rx;
  struct e1000_tx_desc *tx;
  uint8_t *rx_buf[E1000_RX_DESCS];
  uint8_t *tx_buf[E1000_TX_DESCS];
  uint32_t rx_next;  /* next descriptor the card fills */
  uint32_t tx_tail;  /* next descriptor we fill */
  uint32_t tx_clean; /* oldest descriptor the card may still own */
  bool irq;          /* interrupts on (else only polled) */
};

static struct e1000 *cards[E1000_MAX_CARDS];
static int ncards;

static inline uint32_t rd(struct e1000 *nic, uint32_t reg) {
  return nic->mmio[reg / 4];
}

static inline void wr(struct e1000 *nic, uint32_t reg, uint32_t val) {
  nic->mmio[reg / 4] = val;
}

/* -- Memory ------------------------------------------------ */

static int map_mmio(uint64_t phys, uint64_t size) {
  for (uint64_t page = phys & ~0xFFFULL; page < phys + size; page += 0x1000) {
    uint64_t virt = phys_to_virt(page);
    if (vmm_is_mapped(virt))
      continue;
    if (vmm_map_page(virt, page, VMM_MAP_NO_CACHE) < 0)
      return -1;
  }
  return 0;
}

/* A zeroed page for DMA: its kernel address, its physical one in @p phys. */
static void *dma_page(uint64_t *phys) {
  uint64_t p = pmm_alloc_page();
  if (!p)
    return NULL;
  void *v = (void *)phys_to_virt(p);
  memset(v, 0, 0x1000);
  *phys = p;
  return v;
}

/* -- EEPROM and MAC address ------------------------------- */

static bool eeprom_read(struct e1000 *nic, uint8_t word, uint16_t *out) {
  uint32_t start, done;
  if (nic->kind == E1000_82574) {
    start = 1u | ((uint32_t)word << 2);
    done = 1u << 1;
  } else {
    start = 1u | ((uint32_t)word << 8);
    done = 1u << 4;
  }
  wr(nic, E1000_EERD, start);
  for (int i = 0; i < 1000; i++) {
    uint32_t v = rd(nic, E1000_EERD);
    if (v & done) {
      *out = (uint16_t)(v >> 16);
      return true;
    }
    hpet_delay_ms(1);
  }
  return false;
}

static bool read_mac(struct e1000 *nic, uint8_t mac[6]) {
  /* The card loads its address into RAL0/RAH0 from the EEPROM at reset. */
  uint32_t ral = rd(nic, E1000_RAL0), rah = rd(nic, E1000_RAH0);
  if (rah & E1000_RAH_AV) {
    for (int i = 0; i < 4; i++)
      mac[i] = (uint8_t)(ral >> (8 * i));
    mac[4] = (uint8_t)rah;
    mac[5] = (uint8_t)(rah >> 8);
    return true;
  }
  for (uint8_t w = 0; w < 3; w++) {
    uint16_t v;
    if (!eeprom_read(nic, w, &v))
      return false;
    mac[w * 2] = (uint8_t)v;
    mac[w * 2 + 1] = (uint8_t)(v >> 8);
  }
  return true;
}

/* -- Rings ------------------------------------------------ */

static bool rings_init(struct e1000 *nic) {
  uint64_t rx_phys, tx_phys;
  /* 64 descriptors of 16 bytes: one page each. */
  nic->rx = dma_page(&rx_phys);
  nic->tx = dma_page(&tx_phys);
  if (!nic->rx || !nic->tx)
    return false;

  for (int i = 0; i < E1000_RX_DESCS; i += 2) {
    uint64_t phys;
    uint8_t *page = dma_page(&phys);
    if (!page)
      return false;
    nic->rx_buf[i] = page;
    nic->rx_buf[i + 1] = page + E1000_BUF_SIZE;
    nic->rx[i].addr = phys;
    nic->rx[i + 1].addr = phys + E1000_BUF_SIZE;
  }
  for (int i = 0; i < E1000_TX_DESCS; i += 2) {
    uint64_t phys;
    uint8_t *page = dma_page(&phys);
    if (!page)
      return false;
    nic->tx_buf[i] = page;
    nic->tx_buf[i + 1] = page + E1000_BUF_SIZE;
    nic->tx[i].addr = phys;
    nic->tx[i + 1].addr = phys + E1000_BUF_SIZE;
    nic->tx[i].status = nic->tx[i + 1].status = E1000_TX_STAT_DD; /* free */
  }

  wr(nic, E1000_RDBAL, (uint32_t)rx_phys);
  wr(nic, E1000_RDBAH, (uint32_t)(rx_phys >> 32));
  wr(nic, E1000_RDLEN, E1000_RX_DESCS * sizeof(struct e1000_rx_desc));
  wr(nic, E1000_RDH, 0);
  wr(nic, E1000_RDT, E1000_RX_DESCS - 1);
  nic->rx_next = 0;
  wr(nic, E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SECRC);

  wr(nic, E1000_TDBAL, (uint32_t)tx_phys);
  wr(nic, E1000_TDBAH, (uint32_t)(tx_phys >> 32));
  wr(nic, E1000_TDLEN, E1000_TX_DESCS * sizeof(struct e1000_tx_desc));
  wr(nic, E1000_TDH, 0);
  wr(nic, E1000_TDT, 0);
  nic->tx_tail = nic->tx_clean = 0;
  wr(nic, E1000_TCTL,
     E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT | E1000_TCTL_COLD);
  wr(nic, E1000_TIPG, E1000_TIPG_DEFAULT);
  return true;
}

/* -- netdev ops (net lock held) --------------------------- */

static int e1000_xmit(struct netdev *dev, const void *frame, size_t len) {
  struct e1000 *nic = dev->priv;
  if (len > E1000_BUF_SIZE)
    return -1;
  struct e1000_tx_desc *d = &nic->tx[nic->tx_tail];
  if (!(d->status & E1000_TX_STAT_DD))
    return -1; /* the ring is full: the card has not sent this one yet */

  memcpy(nic->tx_buf[nic->tx_tail], frame, len);
  if (len < 60) /* the minimum frame, without the CRC the card adds */
    memset(nic->tx_buf[nic->tx_tail] + len, 0, 60 - len), len = 60;
  d->length = (uint16_t)len;
  d->cso = 0;
  d->css = 0;
  d->special = 0;
  d->cmd = E1000_TX_CMD_EOP | E1000_TX_CMD_IFCS | E1000_TX_CMD_RS;
  d->status = 0;
  __atomic_thread_fence(__ATOMIC_SEQ_CST); /* descriptor before the tail */
  nic->tx_tail = (nic->tx_tail + 1) % E1000_TX_DESCS;
  wr(nic, E1000_TDT, nic->tx_tail);
  return 0;
}

static void e1000_poll(struct netdev *dev) {
  struct e1000 *nic = dev->priv;
  for (int budget = E1000_RX_DESCS; budget > 0; budget--) {
    struct e1000_rx_desc *d = &nic->rx[nic->rx_next];
    if (!(d->status & E1000_RX_STAT_DD))
      break;
    __atomic_thread_fence(__ATOMIC_SEQ_CST); /* status before the data */
    /* Frames longer than a buffer (never with a 1500 MTU) and bad ones
     * are dropped. */
    if ((d->status & E1000_RX_STAT_EOP) && !d->errors)
      netdev_rx(dev, nic->rx_buf[nic->rx_next], d->length);
    d->status = 0;
    /* Give it back: the tail is the last descriptor the card may fill. */
    wr(nic, E1000_RDT, nic->rx_next);
    nic->rx_next = (nic->rx_next + 1) % E1000_RX_DESCS;
  }
}

static bool e1000_link_up(struct netdev *dev) {
  return rd(dev->priv, E1000_STATUS) & E1000_STATUS_LU;
}

static const struct netdev_ops e1000_ops = {
    .xmit = e1000_xmit,
    .poll = e1000_poll,
    .link_up = e1000_link_up,
};

/* -- Interrupts -------------------------------------------- */

/* The line each card asserts (PCI INTx) wakes the net thread, which then
 * polls; the handler only acknowledges the card (reading ICR clears its
 * causes and drops the line). One handler serves every card, as several
 * may share a line. A line the cards keep firing without a cause of
 * theirs is someone else's too (it was free when we took it, so a device
 * without a driver): it is masked and the cards are only polled. */

#define E1000_SPURIOUS_MAX 10000

static uint8_t irq_lines[E1000_MAX_CARDS]; /* lines we hold */
static int nlines;
static int spurious;

static void e1000_irq(registers_t *regs) {
  bool ours = false;
  for (int i = 0; i < ncards; i++) {
    struct e1000 *nic = cards[i];
    if (nic->irq && (rd(nic, E1000_ICR) & E1000_IRQ_CAUSES)) {
      netdev_wake(&nic->netdev);
      ours = true;
    }
  }
  if (ours) {
    spurious = 0;
  } else if (++spurious == E1000_SPURIOUS_MAX) {
    uint8_t line = (uint8_t)(regs->int_no - 32);
    ioapic_mask_irq(line);
    printk(KERN_WARNING "[e1000] IRQ %u fires for another device; "
                        "polling instead\n",
           line);
  }
}

/* Take the card's INTx line (shared with another of our cards is fine);
 * without it the card is polled, which works, a tick slower. */
static void irq_setup(struct e1000 *nic, struct pci_device *pci) {
  uint32_t r = pci_read_config(pci->bus, pci->slot, pci->func, 0x3C);
  uint8_t line = (uint8_t)r, pin = (uint8_t)(r >> 8);
  if (!pin || line == 0xFF)
    return; /* no INTx, or the firmware routed it nowhere */

  bool held = false;
  for (int i = 0; i < nlines; i++)
    held |= irq_lines[i] == line;
  if (!held) {
    if (ioapic_request_level_irq(line, e1000_irq) < 0)
      return;
    irq_lines[nlines++] = line;
  }
  nic->irq = true;
  wr(nic, E1000_RDTR, 0); /* interrupt at once for each frame */
  (void)rd(nic, E1000_ICR);
  wr(nic, E1000_IMS, E1000_IRQ_CAUSES);
}

/* -- Probe ------------------------------------------------- */

static int e1000_probe(struct pci_device *pci) {
  uint16_t device = pci_read_config(pci->bus, pci->slot, pci->func, 0) >> 16;
  int card = -1;
  for (size_t i = 0; i < sizeof(e1000_cards) / sizeof(e1000_cards[0]); i++)
    if (e1000_cards[i].device == device)
      card = (int)i;
  if (card < 0 || ncards == E1000_MAX_CARDS || !pci->bar0)
    return -1;

  /* Memory space and bus mastering (the card DMAs to our rings). */
  uint32_t cmd = pci_read_config(pci->bus, pci->slot, pci->func, 0x04);
  pci_write_config(pci->bus, pci->slot, pci->func, 0x04, cmd | (1 << 1) | (1 << 2));
  if (map_mmio(pci->bar0, E1000_MMIO_SIZE) < 0) {
    printk(KERN_ERR "[e1000] cannot map registers at %lx\n", pci->bar0);
    return -1;
  }

  struct e1000 *nic = kzalloc(sizeof(*nic));
  if (!nic)
    return -1;
  nic->mmio = (volatile uint32_t *)phys_to_virt(pci->bar0);
  nic->kind = e1000_cards[card].kind;

  /* Reset, with interrupts masked before and after. */
  wr(nic, E1000_IMC, 0xFFFFFFFF);
  wr(nic, E1000_CTRL, rd(nic, E1000_CTRL) | E1000_CTRL_RST);
  hpet_delay_ms(1);
  for (int i = 0; i < 100 && (rd(nic, E1000_CTRL) & E1000_CTRL_RST); i++)
    hpet_delay_ms(1);
  wr(nic, E1000_IMC, 0xFFFFFFFF);
  (void)rd(nic, E1000_ICR);
  if (nic->kind == E1000_82574) /* wait for the EEPROM to be loaded */
    for (int i = 0; i < 100 && !(rd(nic, E1000_EECD) & E1000_EECD_AUTO_RD); i++)
      hpet_delay_ms(1);

  /* Link: auto-negotiated speed, set up. */
  uint32_t ctrl = rd(nic, E1000_CTRL);
  ctrl &= ~(E1000_CTRL_ILOS | E1000_CTRL_VME | E1000_CTRL_PHY_RST);
  wr(nic, E1000_CTRL, ctrl | E1000_CTRL_SLU | E1000_CTRL_ASDE);

  struct netdev *dev = &nic->netdev;
  if (!read_mac(nic, dev->mac)) {
    printk(KERN_ERR "[e1000] cannot read the MAC address\n");
    kfree(nic);
    return -1;
  }
  wr(nic, E1000_RAL0,
     dev->mac[0] | dev->mac[1] << 8 | dev->mac[2] << 16 |
         (uint32_t)dev->mac[3] << 24);
  wr(nic, E1000_RAH0, dev->mac[4] | dev->mac[5] << 8 | E1000_RAH_AV);
  for (int i = 0; i < 128; i++)
    wr(nic, E1000_MTA + i * 4, 0);

  if (!rings_init(nic)) {
    printk(KERN_ERR "[e1000] out of memory for the rings\n");
    kfree(nic); /* the pages stay allocated; a probe failing here is rare */
    return -1;
  }

  dev->mtu = 1500;
  dev->ops = &e1000_ops;
  dev->priv = nic;
  int err = netdev_register(dev);
  if (err < 0) {
    printk(KERN_ERR "[e1000] cannot register the card (%d)\n", err);
    wr(nic, E1000_RCTL, 0);
    wr(nic, E1000_TCTL, 0);
    kfree(nic);
    return -1;
  }
  cards[ncards++] = nic;
  irq_setup(nic, pci);
  printk(KERN_OK "[e1000] %s: Intel %s at %02x:%02x.%d, link %s, %s\n",
         dev->name, e1000_cards[card].name, pci->bus, pci->slot, pci->func,
         e1000_link_up(dev) ? "up" : "down", nic->irq ? "IRQ" : "polled");
  return 0;
}

/* -- Driver registration ---------------------------------- */

#define E1000_ID(dev) {.vendor = 0x8086, .device = (dev)}

static const struct pci_device_id e1000_ids[] = {
    E1000_ID(0x100E), E1000_ID(0x100F), E1000_ID(0x1004), E1000_ID(0x100C),
    E1000_ID(0x1015), E1000_ID(0x1019), E1000_ID(0x101E), E1000_ID(0x1026),
    E1000_ID(0x107C), E1000_ID(0x10D3), E1000_ID(0x10F6), {0},
};

static struct pci_driver e1000_driver = {
    .name = "e1000",
    .id_table = e1000_ids,
    .probe = e1000_probe,
};

static int e1000_module_init(void) { return pci_register_driver(&e1000_driver); }

module_init(e1000_module_init);
MODULE_NAME("e1000");
