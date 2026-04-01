#include "uhci-hcd.h"

#include <hubble/init.h>
#include <hubble/printk.h>

#include <drivers/pci/pci.h>
#include <io.h>

#include <hpet/hpet.h>

struct uhci_hcd
{
	uint16_t io_base;	   // base I/O address
	uint32_t *frame_list_virt; // Frame List virtual address
	uint64_t frame_list_phys;  // physical address Frame List
	int num_ports;		   // number of ports
};

static void uhci_reset_controller(uint16_t io_base)
{
	uint16_t cmd;

	// 1. Host Controller Reset
	cmd = inw(io_base + USBCMD);
	cmd |= HCRESET;
	outw(cmd, io_base + USBCMD);

	// wait until HCRESET is reset
	while (inw(io_base + USBCMD) & HCRESET)
		; // просто ждем

	printk("[uhci] Host Controller Reset done\n");

	// 2. Global Reset
	cmd = inw(io_base + USBCMD);
	cmd |= GRESET;
	outw(cmd, io_base + USBCMD);

	// wait 10 ms
	// delay_ms(10);
	hpet_delay_ms(10);

	// remove the GRESET bit
	cmd &= ~GRESET;
	outw(cmd, io_base + USBCMD);

	printk("[uhci] Global Reset done\n");
}

#include "uhci-hcd.h"
#include <drivers/pci/pci.h>
#include <io.h>
#include <hubble/printk.h>
#include <mm/pmm.h>
#include <higher_half.h>

#define FRAME_LIST_SIZE 1024

static uint32_t *frame_list_virt = NULL;
static uint64_t frame_list_phys = 0;

static int uhci_init_frame_list(uint16_t io_base)
{
	// allocate 1 page (4096 bytes) aligned to 4K
	frame_list_phys = pmm_alloc_page();
	if (!frame_list_phys)
	{
		printk("[uhci] failed to allocate frame list page\n");
		return -1;
	}

	// get the virtual address for the CPU
	frame_list_virt = PHYS_TO_VIRT_PTR(uint32_t, frame_list_phys);

	// fill with empty records (T-bit = 1)
	for (int i = 0; i < FRAME_LIST_SIZE; i++)
		frame_list_virt[i] = 0x00000001;

	// write the physical address of the Frame List to the UHCI register
	outl((uint32_t)frame_list_phys, io_base + FRBASEADD);

	printk("[uhci] frame list allocated phys=0x%llx virt=%p\n",
	       frame_list_phys, frame_list_virt);

	return 0;
}

#define USBINTR_TO (1 << 0)  // Timeout/Short Packet
#define USBINTR_IOC (1 << 2) // Interrupt on Complete
#define USBINTR_RES (1 << 1) // Resume detect
#define USBINTR_HSE (1 << 3) // Host system error

static void uhci_enable_interrupts(uint16_t io_base)
{
	uint16_t intr = USBINTR_TO | USBINTR_IOC | USBINTR_RES | USBINTR_HSE;
	outw(intr, io_base + USBINTR);
	printk("[uhci] USB interrupts enabled: 0x%x\n", intr);
}

static int uhci_count_ports(uint16_t io_base)
{
	int port = 0;
	while (1)
	{
		uint16_t portsc = inw(io_base + PORTSC1 + port * 2);

		// if ((portsc & (1 << 7)) || portsc == 0xFFFF)
		// 	break;
		if (portsc == 0xFFFF)
			break;

		port++;
	}
	printk("[uhci] found %d ports\n", port);
	return port;
}

#define USBCMD_RS (1 << 0)  // Run/Stop
#define USBCMD_64B (1 << 7) // Enable 64-byte transfers

static void uhci_start_controller(uint16_t io_base)
{
	uint16_t cmd = inw(io_base + USBCMD);

	cmd |= USBCMD_RS | USBCMD_64B; // turn on controller + 64 байт
	outw(cmd, io_base + USBCMD);

	printk("[uhci] controller started (RS + 64B)\n");
}

#define PORTSC_CSC (1 << 0)   // Connection Status Change
#define PORTSC_CS (1 << 0)    // Connection Status (actual status)
#define PORTSC_PE (1 << 2)    // Port Enable
#define PORTSC_RESET (1 << 8) // Port Reset

static void uhci_check_ports(struct uhci_hcd *hcd)
{
	for (int i = 0; i < hcd->num_ports; i++)
	{
		uint16_t portsc = inw(hcd->io_base + PORTSC1 + i * 2);

		if (portsc & PORTSC_CSC)
		{
			printk("[uhci] port %d change detected\n", i + 1);

			// 1. Immediately clear the CSC bit with a separate entry
			outw(PORTSC_CSC, hcd->io_base + PORTSC1 + i * 2);

			// 2. Checking if the device is connected
			if (portsc & PORTSC_CS)
			{
				printk("[uhci] device connected on port %d\n", i + 1);

				// 3. We turn on the device in sequence
				outw(PORTSC_RESET, hcd->io_base + PORTSC1 + i * 2); // Reset
				hpet_delay_ms(100);
				outw(0, hcd->io_base + PORTSC1 + i * 2); // Clear Reset
				hpet_delay_ms(50);
				outw(PORTSC_PE, hcd->io_base + PORTSC1 + i * 2); // Device Enable

				// wait until the PE bit is set
				while (!(inw(hcd->io_base + PORTSC1 + i * 2) & PORTSC_PE))
					;
				printk("[uhci] device enabled on port %d\n", i + 1);
			}
			else
			{
				printk("[uhci] device disconnected from port %d\n", i + 1);
				// You can free TD/QH, Frame List, etc. here.
			}
		}
	}
}

static void uhci_root_hub_poll(struct uhci_hcd *hcd)
{
	while (1)
	{
		uhci_check_ports(hcd);
		hpet_delay_ms(100); // check every 100 ms
	}
}

static int uhci_probe(struct pci_device *pci_dev)
{
	struct uhci_hcd *hcd = (struct uhci_hcd *)pmm_alloc_page();
	if (!hcd)
		return -1;

	uint16_t io_base = 0;

	// Search I/O BAR
	for (int i = 0; i < 6; i++)
	{
		uint32_t bar = pci_read_config(pci_dev->bus, pci_dev->slot, pci_dev->func,
					       PCI_BAR0 + i * sizeof(uint32_t));
		if (bar & PCI_BAR_IO)
		{
			io_base = (uint16_t)(bar & PCI_BAR_IO_MASK);
			printk("[uhci] found I/O BAR%d = 0x%x\n", i, io_base);
			break;
		}
	}

	if (!io_base)
		return -1;

	pci_set_command(pci_dev, PCI_COMMAND_IO | PCI_COMMAND_MASTER);

	hcd->io_base = io_base;
	uhci_reset_controller(io_base);
	uhci_init_frame_list(io_base);
	uhci_enable_interrupts(io_base);
	uhci_start_controller(io_base);
	hcd->num_ports = uhci_count_ports(io_base);

	uint16_t usbsts = inw(io_base + USBSTS);
	printk("[uhci] USBSTS = 0x%x\n", usbsts);

	uhci_root_hub_poll(hcd);

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
