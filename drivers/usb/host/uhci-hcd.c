#include "uhci-hcd.h"

#include <hubble/init.h>
#include <hubble/printk.h>

#include <drivers/pci/pci.h>
#include <io.h>

static volatile uint16_t uhci_io_base = 0;

static int uhci_probe(struct pci_device *pci_dev)
{
	uint16_t io_base = 0;

	/* Search I/O BAR */
	for (int i = 0; i < 6; i++)
	{
		uint32_t bar = pci_read_config(
		    pci_dev->bus,
		    pci_dev->slot,
		    pci_dev->func,
		    PCI_BAR0 + i * sizeof(uint32_t));

		if (bar & PCI_BAR_IO)
		{
			io_base = (uint16_t)(bar & PCI_BAR_IO_MASK);
			printk("[uhci] found I/O BAR%d = 0x%x\n", i, io_base);
			break;
		}
	}

	if (!io_base)
	{
		printk("[uhci] no I/O BAR found\n");
		return -1;
	}

	/* Turn I/O + Bus Master */
	pci_set_command(pci_dev, PCI_COMMAND_IO | PCI_COMMAND_MASTER);

	/* Read status UHCI */
	uint16_t usbsts = inw(io_base + USB_STATUS);
	printk("[uhci] USBSTS = 0x%x\n", usbsts);

	return 0;
}

static const struct pci_device_id uhci_ids[] = {
    {0x8086, 0x7020}, // PIIX3 UHCI
    {0x8086, 0x7112}, // PIIX4 UHCI
    {0, 0},
};

static struct pci_driver uhci_driver = {
    .name = "uhci",
    .id_table = uhci_ids,
    .probe = uhci_probe,
};

__init int uhci_module_init(void)
{
	printk("[uhci] module init\n");
	return pci_register_driver(&uhci_driver);
}

device_initcall(uhci_module_init);
