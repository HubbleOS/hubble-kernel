/**
 * @file pci.c
 * @brief PCI configuration-space access and driver registration
 */
#include "pci.h"
#include <hubble/printk.h>
#include <io.h>
#include <mm/kmalloc.h>
#include <stdbool.h>
#include <stdint.h>

/* -- Configuration space helpers -------------------------- */

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func,
                         uint8_t offset) {
  uint32_t addr = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                  ((uint32_t)func << 8) | (offset & 0xFC);

  outl(0xCF8, addr);
  return inl(0xCFC);
}

void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset,
                      uint32_t val) {
  uint32_t addr = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                  ((uint32_t)func << 8) | (offset & 0xFC);

  outl(0xCF8, addr);
  outl(0xCFC, val);
}

/* -- Command register helpers ----------------------------- */

void pci_set_command(struct pci_device *dev, uint16_t flags) {
  uint32_t cmd = pci_read_config(dev->bus, dev->slot, dev->func, PCI_COMMAND);
  cmd |= flags;
  pci_write_config(dev->bus, dev->slot, dev->func, PCI_COMMAND, cmd);
}

/* -- Device structure allocation -------------------------- */

static struct pci_device *allocate_pci_device_struct(uint8_t bus, uint8_t slot,
                                                     uint8_t func) {
  struct pci_device *dev = kmalloc(sizeof(struct pci_device), GFP_KERNEL);

  dev->bus = bus;
  dev->slot = slot;
  dev->func = func;

  uint32_t reg = pci_read_config(bus, slot, func, PCI_REVISION_ID);

  dev->class_code = (PCI_GET_CLASS(reg) << 16) | (PCI_GET_SUBCLASS(reg) << 8) |
                    PCI_GET_PROGIF(reg);

  uint32_t bar0 = pci_read_config(bus, slot, func, PCI_BAR0);
  dev->bar0 = bar0 & PCI_BAR_ADDR_MASK;

  /* A 64-bit memory BAR keeps its upper half in the next BAR register. */
  if (!(bar0 & PCI_BAR_IO) && (bar0 & PCI_BAR_MEM_TYPE) == PCI_BAR_MEM_64)
    dev->bar0 |= (uint64_t)pci_read_config(bus, slot, func, PCI_BAR1) << 32;

  return dev;
}

static bool pci_id_matches(const struct pci_device_id *id, uint16_t vendor,
                           uint16_t device, uint32_t class_code) {
  if (id->class_mask)
    return (class_code & id->class_mask) == id->class_code;
  return id->vendor == vendor && id->device == device;
}

/* -- Driver registration and bus scan --------------------- */

int pci_register_driver(struct pci_driver *drv) {
  printk(KERN_INFO "[pci] registering driver: %s\n", drv->name);

  for (uint16_t bus = 0; bus < 256; bus++) {
    for (uint8_t slot = 0; slot < 32; slot++) {
      for (uint8_t func = 0; func < 8; func++) {
        uint32_t reg = pci_read_config(bus, slot, func, PCI_VENDOR_ID);

        uint16_t vendor = PCI_GET_VENDOR(reg);
        if (vendor == 0xFFFF || vendor == 0x0000)
          continue;

        uint16_t device = PCI_GET_DEVICE(reg);
        uint32_t rev = pci_read_config(bus, slot, func, PCI_REVISION_ID);
        uint32_t class_code = (PCI_GET_CLASS(rev) << 16) |
                              (PCI_GET_SUBCLASS(rev) << 8) |
                              PCI_GET_PROGIF(rev);

        for (const struct pci_device_id *id = drv->id_table;
             id->vendor || id->class_mask; id++) {
          if (!pci_id_matches(id, vendor, device, class_code))
            continue;

          struct pci_device *dev = allocate_pci_device_struct(bus, slot, func);

          printk(KERN_INFO "[pci] %s matched at %02x:%02x.%d\n", drv->name, bus,
                 slot, func);

          drv->probe(dev);
        }
      }
    }
  }

  return 0;
}
