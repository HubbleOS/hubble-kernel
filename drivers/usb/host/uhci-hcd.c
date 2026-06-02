#include "uhci-hcd.h"
#include "uhci-td.h"

#include <hubble/init.h>
#include <hubble/printk.h>
#include <drivers/pci/pci.h>
#include <io.h>
#include <hpet/hpet.h>
#include <mm/pmm.h>
#include <higher_half.h>

#define cpu_relax() asm volatile("pause" ::: "memory")

struct uhci_hcd
{
	uint16_t io_base;
	uint32_t *frame_list_virt;
	uint64_t frame_list_phys;
	int num_ports;
};

static int uhci_set_address(struct uhci_hcd *hcd, uint8_t new_addr);
static int uhci_get_device_descriptor(struct uhci_hcd *hcd,
				      uint8_t dev_addr,
				      int is_low_speed);
static int uhci_set_configuration(struct uhci_hcd *hcd, uint8_t dev_addr, uint8_t config);
static int uhci_interrupt_transfer(struct uhci_hcd *hcd, uint8_t dev_addr);
static int uhci_wait_td(struct uhci_td *td, char *label);
/* ────────────────────────────────────────────────────────
 * Reset
 * ──────────────────────────────────────────────────────── */
static void uhci_reset_controller(uint16_t io_base)
{
	uint16_t cmd;

	cmd = inw(io_base + USBCMD);
	cmd |= HCRESET;
	outw(io_base + USBCMD, cmd);
	while (inw(io_base + USBCMD) & HCRESET)
		;
	printk(KERN_INFO "[uhci] Host Controller Reset done\n");

	cmd = inw(io_base + USBCMD);
	cmd |= GRESET;
	outw(io_base + USBCMD, cmd);
	hpet_delay_ms(10);
	cmd &= ~GRESET;
	outw(io_base + USBCMD, cmd);
	printk(KERN_INFO "[uhci] Global Reset done\n");
}

/* ────────────────────────────────────────────────────────
 * Frame List
 * ──────────────────────────────────────────────────────── */
#define FRAME_LIST_SIZE 1024

static int uhci_init_frame_list(struct uhci_hcd *hcd)
{
	hcd->frame_list_phys = pmm_alloc_page();
	if (!hcd->frame_list_phys)
	{
		printk(KERN_ERR "[uhci] failed to allocate frame list\n");
		return -1;
	}

	hcd->frame_list_virt = (uint32_t *)phys_to_virt(hcd->frame_list_phys);

	for (int i = 0; i < FRAME_LIST_SIZE; i++)
		hcd->frame_list_virt[i] = 0x00000001;

	outl(hcd->io_base + FRBASEADD, (uint32_t)hcd->frame_list_phys);

	printk(KERN_INFO "[uhci] frame list phys=0x%llx virt=%p\n",
	       hcd->frame_list_phys, hcd->frame_list_virt);
	return 0;
}

/* ────────────────────────────────────────────────────────
 * Interrupts
 * ──────────────────────────────────────────────────────── */
#define USBINTR_TO (1 << 0)
#define USBINTR_RES (1 << 1)
#define USBINTR_IOC (1 << 2)
#define USBINTR_HSE (1 << 3)

static void uhci_enable_interrupts(uint16_t io_base)
{
	uint16_t intr = USBINTR_TO | USBINTR_RES | USBINTR_IOC | USBINTR_HSE;
	outw(io_base + USBINTR, intr);
	printk(KERN_OK "[uhci] interrupts enabled: 0x%x\n", intr);
}

/* ────────────────────────────────────────────────────────
 * Start
 * ──────────────────────────────────────────────────────── */
#define USBCMD_RS (1 << 0)

static void uhci_start_controller(uint16_t io_base)
{
	outw(io_base + USBCMD, 0);
	hpet_delay_ms(10);

	printk(KERN_INFO "[uhci] writing RS...\n");
	outw(io_base + USBCMD, USBCMD_RS);

	// читаємо одразу після запису
	uint16_t cmd_imm = inw(io_base + USBCMD);
	printk(KERN_INFO "[uhci] USBCMD immediately after write = 0x%x\n", cmd_imm);

	hpet_delay_ms(10);

	uint16_t cmd = inw(io_base + USBCMD);
	uint16_t sts = inw(io_base + USBSTS);
	printk(KERN_INFO "[uhci] USBCMD=0x%x USBSTS=0x%x\n", cmd, sts);

	if (sts & (1 << 5))
		printk(KERN_WARNING "[uhci] WARNING: HCHalted still set!\n");
	else
		printk(KERN_INFO "[uhci] controller running\n");
}

/* ────────────────────────────────────────────────────────
 * Ports
 * ──────────────────────────────────────────────────────── */
static int uhci_count_ports(uint16_t io_base)
{
	int port = 0;
	while (1)
	{
		uint16_t portsc = inw(io_base + PORTSC1 + port * 2);
		if (portsc == 0xFFFF || (portsc & (1 << 15)))
			break;
		port++;
	}
	printk(KERN_INFO "[uhci] found %d ports\n", port);
	return port;
}

/* ────────────────────────────────────────────────────────
 * Descriptor structures
 * ──────────────────────────────────────────────────────── */
struct usb_config_descriptor
{
	uint8_t bLength;
	uint8_t bDescriptorType; // 0x02
	uint16_t wTotalLength;	 // повна довжина з усіма вкладеними
	uint8_t bNumInterfaces;
	uint8_t bConfigurationValue;
	uint8_t iConfiguration;
	uint8_t bmAttributes;
	uint8_t bMaxPower;
} __attribute__((packed));

struct usb_interface_descriptor
{
	uint8_t bLength;
	uint8_t bDescriptorType; // 0x04
	uint8_t bInterfaceNumber;
	uint8_t bAlternateSetting;
	uint8_t bNumEndpoints;
	uint8_t bInterfaceClass;    // 0x03 = HID
	uint8_t bInterfaceSubClass; // 0x01 = Boot Interface
	uint8_t bInterfaceProtocol; // 0x01 = keyboard, 0x02 = mouse
	uint8_t iInterface;
} __attribute__((packed));

struct usb_endpoint_descriptor
{
	uint8_t bLength;
	uint8_t bDescriptorType;  // 0x05
	uint8_t bEndpointAddress; // біт 7: 0=OUT, 1=IN; біти 3:0 = номер EP
	uint8_t bmAttributes;	  // біти 1:0: 0=Control,1=Iso,2=Bulk,3=Interrupt
	uint16_t wMaxPacketSize;
	uint8_t bInterval; // інтервал поллінгу в мс
} __attribute__((packed));

#define USB_DESC_CONFIGURATION 0x02
#define USB_DESC_INTERFACE 0x04
#define USB_DESC_ENDPOINT 0x05
#define USB_CLASS_HID 0x03
#define USB_PROTOCOL_KEYBOARD 0x01
#define USB_PROTOCOL_MOUSE 0x02

#define CONFIG_DESC_BUF_SIZE 255

/* ────────────────────────────────────────────────────────
 * GET_DESCRIPTOR (Configuration Descriptor)
 * Повертає protocol: 1=keyboard, 2=mouse, 0=інший/невідомий
 * ──────────────────────────────────────────────────────── */
static int uhci_get_config_descriptor(struct uhci_hcd *hcd,
				      uint8_t dev_addr,
				      int is_low_speed,
				      uint8_t *out_protocol,
				      uint8_t *out_endpoint,
				      uint16_t *out_max_packet)
{
	uint64_t setup_phys = pmm_alloc_page();
	struct usb_setup_packet *pkt = (struct usb_setup_packet *)phys_to_virt(setup_phys);
	pkt->bmRequestType = 0x80;
	pkt->bRequest = 0x06;			     // GET_DESCRIPTOR
	pkt->wValue = (USB_DESC_CONFIGURATION << 8); // Config Descriptor, index 0
	pkt->wIndex = 0;
	pkt->wLength = CONFIG_DESC_BUF_SIZE;

	uint64_t buf_phys = pmm_alloc_page();
	uint8_t *buf = (uint8_t *)phys_to_virt(buf_phys);

	uint32_t ls = is_low_speed ? TD_STATUS_LS : 0;

	uint64_t td_phys = pmm_alloc_page();
	struct uhci_td *td = (struct uhci_td *)phys_to_virt(td_phys);

	/* td[0]: SETUP */
	td[0].link = (uint32_t)(td_phys + sizeof(struct uhci_td)) | TD_LINK_DEPTH;
	td[0].status = TD_STATUS_ERRCNT(3) | TD_STATUS_ACTIVE | ls;
	td[0].token = TD_TOKEN(TD_PID_SETUP, dev_addr, 0, 0, 7);
	td[0].buffer = (uint32_t)setup_phys;

	/* td[1]: IN — читаємо дескриптор */
	td[1].link = (uint32_t)(td_phys + 2 * sizeof(struct uhci_td)) | TD_LINK_DEPTH;
	td[1].status = TD_STATUS_ERRCNT(3) | TD_STATUS_ACTIVE | ls;
	td[1].token = TD_TOKEN(TD_PID_IN, dev_addr, 0, 1, CONFIG_DESC_BUF_SIZE - 1);
	td[1].buffer = (uint32_t)buf_phys;

	/* td[2]: STATUS */
	td[2].link = TD_LINK_TERMINATE;
	td[2].status = TD_STATUS_ERRCNT(3) | TD_STATUS_ACTIVE | ls;
	td[2].token = TD_TOKEN(TD_PID_OUT, dev_addr, 0, 1, 0x7FF);
	td[2].buffer = 0;

	uint64_t qh_phys = pmm_alloc_page();
	struct uhci_qh *qh = (struct uhci_qh *)phys_to_virt(qh_phys);
	qh->head_link = TD_LINK_TERMINATE;
	qh->element_link = (uint32_t)td_phys;

	for (int f = 0; f < FRAME_LIST_SIZE; f++)
		hcd->frame_list_virt[f] = (uint32_t)qh_phys | TD_LINK_QH;

	if (uhci_wait_td(&td[0], "GET_CFG td0") < 0)
		return -1;
	if (uhci_wait_td(&td[1], "GET_CFG td1") < 0)
		return -1;
	if (uhci_wait_td(&td[2], "GET_CFG td2") < 0)
		return -1;

	if (td[1].status & TD_STATUS_STALLED)
	{
		printk(KERN_ERR "[uhci] GET_CONFIG stalled!\n");
		return -1;
	}

	/* парсимо дескриптори */
	struct usb_config_descriptor *cfg = (struct usb_config_descriptor *)buf;
	printk(KERN_INFO "[uhci] config: interfaces=%d total_len=%d\n",
	       cfg->bNumInterfaces, cfg->wTotalLength);

	*out_protocol = 0;
	*out_endpoint = 1;   // дефолт
	*out_max_packet = 8; // дефолт

	uint8_t *p = buf + cfg->bLength;
	uint8_t *end = buf + cfg->wTotalLength;

	while (p < end && p[0] > 0)
	{
		uint8_t len = p[0];
		uint8_t type = p[1];

		if (type == USB_DESC_INTERFACE)
		{
			struct usb_interface_descriptor *iface =
			    (struct usb_interface_descriptor *)p;
			printk(KERN_INFO "[uhci] interface: class=0x%02x subclass=0x%02x proto=0x%02x\n",
			       iface->bInterfaceClass,
			       iface->bInterfaceSubClass,
			       iface->bInterfaceProtocol);

			if (iface->bInterfaceClass == USB_CLASS_HID)
				*out_protocol = iface->bInterfaceProtocol;
		}
		else if (type == USB_DESC_ENDPOINT)
		{
			struct usb_endpoint_descriptor *ep =
			    (struct usb_endpoint_descriptor *)p;
			/* беремо перший IN endpoint */
			if (ep->bEndpointAddress & 0x80)
			{
				*out_endpoint = ep->bEndpointAddress & 0x0F;
				*out_max_packet = ep->wMaxPacketSize;
				printk(KERN_INFO "[uhci] endpoint: addr=0x%02x maxpkt=%d interval=%d\n",
				       ep->bEndpointAddress,
				       ep->wMaxPacketSize,
				       ep->bInterval);
			}
		}

		p += len;
	}

	return 0;
}

#define KBD_REPORT_SIZE 8

static int uhci_keyboard_transfer(struct uhci_hcd *hcd,
				  uint8_t dev_addr,
				  int is_low_speed)
{
	uint32_t ls = is_low_speed ? TD_STATUS_LS : 0;
	uint64_t buf_phys = pmm_alloc_page();
	uint8_t *buf = (uint8_t *)phys_to_virt(buf_phys);

	uint64_t td_phys = pmm_alloc_page();
	struct uhci_td *td = (struct uhci_td *)phys_to_virt(td_phys);

	uint64_t qh_phys = pmm_alloc_page();
	struct uhci_qh *qh = (struct uhci_qh *)phys_to_virt(qh_phys);
	qh->head_link = TD_LINK_TERMINATE;
	qh->element_link = (uint32_t)td_phys;

	for (int f = 0; f < FRAME_LIST_SIZE; f++)
		hcd->frame_list_virt[f] = (uint32_t)qh_phys | TD_LINK_QH;

	uint8_t toggle = 0;

	while (1)
	{
		td->link = TD_LINK_TERMINATE;
		td->status = TD_STATUS_ERRCNT(3) | TD_STATUS_ACTIVE | ls;
		td->token = TD_TOKEN(TD_PID_IN, dev_addr, 1, toggle,
				     KBD_REPORT_SIZE - 1);
		td->buffer = (uint32_t)buf_phys;
		qh->element_link = (uint32_t)td_phys;

		int timeout = 100000;
		while ((td->status & TD_STATUS_ACTIVE) && timeout--)
			cpu_relax();

		if (td->status & TD_STATUS_STALLED)
		{
			printk(KERN_ERR "[uhci] kbd stalled!\n");
			break;
		}

		uint8_t mod = buf[0];
		uint8_t key = buf[2]; // перший keycode

		if (key)
		{
			printk(KERN_INFO "[uhci] key: mod=0x%02x keycode=0x%02x\n", mod, key);
		}

		toggle ^= 1;
	}
	return 0;
}

#define PORTSC_CSC (1 << 1)
#define PORTSC_CS (1 << 0)
#define PORTSC_PE (1 << 2)
#define PORTSC_RESET (1 << 9)

static void uhci_check_ports(struct uhci_hcd *hcd)
{
	for (int i = 0; i < hcd->num_ports; i++)
	{
		uint16_t portsc = inw(hcd->io_base + PORTSC1 + i * 2);

		if (portsc & PORTSC_CS)
		{
			// устройство подключено
			if (portsc & PORTSC_CSC)
			{
				// сбрасываем флаг изменения
				outw(hcd->io_base + PORTSC1 + i * 2, portsc | PORTSC_CSC);
			}

			printk(KERN_INFO "[uhci] device connected on port %d\n", i + 1);

			// сброс, включение порта и установка адреса/конфигурации
			outw(hcd->io_base + PORTSC1 + i * 2, PORTSC_RESET);
			hpet_delay_ms(100);
			outw(hcd->io_base + PORTSC1 + i * 2, 0);
			hpet_delay_ms(10);

			outw(hcd->io_base + PORTSC1 + i * 2, PORTSC_PE);
			hpet_delay_ms(50);

			int timeout = 100;
			while (timeout--)
			{
				uint16_t portsc = inw(hcd->io_base + PORTSC1 + i * 2);
				if (portsc & PORTSC_PE)
					break;
				hpet_delay_ms(1);
			}

			uint16_t portsc = inw(hcd->io_base + PORTSC1 + i * 2);
			printk(KERN_OK "[uhci] port %d enabled, portsc=0x%x\n", i + 1, portsc);

			if (!(portsc & PORTSC_PE))
			{
				printk(KERN_ERR "[uhci] port %d failed to enable!\n", i + 1);
				continue;
			}

			int is_low_speed = (portsc & (1 << 7)) ? 1 : 0;

			if (uhci_get_device_descriptor(hcd, 0, is_low_speed) < 0)
			{
				printk(KERN_ERR "[uhci] port %d: get_descriptor failed\n", i + 1);
				continue;
			}

			uhci_set_address(hcd, i + 1);
			hpet_delay_ms(10);
			uint8_t protocol = 0, endpoint = 1;
			uint16_t max_packet = 8;

			if (uhci_get_config_descriptor(hcd, i + 1, is_low_speed,
						       &protocol, &endpoint, &max_packet) < 0)
			{
				printk(KERN_ERR "[uhci] port %d: get_config failed\n", i + 1);
				continue;
			}

			uhci_set_configuration(hcd, i + 1, 1);
			hpet_delay_ms(10);

			if (protocol == USB_PROTOCOL_MOUSE)
			{
				printk(KERN_INFO "[uhci] -> mouse on port %d\n", i + 1);

				uhci_interrupt_transfer(hcd, i + 1);

				// uhci_mouse_transfer(hcd, i + 1, is_low_speed, endpoint, max_packet);
			}
			else if (protocol == USB_PROTOCOL_KEYBOARD)
			{
				printk(KERN_INFO "[uhci] -> keyboard on port %d\n", i + 1);
				uhci_keyboard_transfer(hcd, i + 1, is_low_speed);
				// uhci_keyboard_transfer(hcd, i + 1, is_low_speed, endpoint, max_packet);
			}
			else
			{
				printk(KERN_INFO "[uhci] -> unknown HID protocol %d\n", protocol);
			}
		}
		else
		{
			printk(KERN_INFO "[uhci] device disconnected from port %d\n", i + 1);
		}
	}
}

static void uhci_root_hub_poll(struct uhci_hcd *hcd)
{
	for (int i = 0; i < hcd->num_ports; i++)
	{
		uint16_t portsc = inw(hcd->io_base + PORTSC1 + i * 2);
		printk(KERN_INFO "[uhci] port %d initial status=0x%x\n", i + 1, portsc);
	}

	uhci_check_ports(hcd);

	// while (1)
	// {
	// 	uhci_check_ports(hcd);
	// 	hpet_delay_ms(100);
	// }
}

static int uhci_wait_td(struct uhci_td *td, char *label)
{
	int timeout = 500000;
	while ((td->status & TD_STATUS_ACTIVE) && timeout--)
		cpu_relax();
	if (timeout <= 0)
	{
		printk(KERN_ERR "[uhci] %s: TD timed out! status=0x%x\n", label, td->status);
		return -1;
	}
	return 0;
}

/* ────────────────────────────────────────────────────────
 * SET_ADDRESS
 * ──────────────────────────────────────────────────────── */
static int uhci_set_address(struct uhci_hcd *hcd, uint8_t new_addr)
{
	uint64_t setup_phys = pmm_alloc_page();
	struct usb_setup_packet *pkt = (struct usb_setup_packet *)phys_to_virt(setup_phys);
	pkt->bmRequestType = 0x00;
	pkt->bRequest = 0x05;
	pkt->wValue = new_addr;
	pkt->wIndex = 0;
	pkt->wLength = 0;

	uint64_t td_phys = pmm_alloc_page();
	struct uhci_td *td = (struct uhci_td *)phys_to_virt(td_phys);

	td[0].link = (uint32_t)(td_phys + sizeof(struct uhci_td)) | TD_LINK_DEPTH;
	td[0].status = TD_STATUS_ERRCNT(3) | TD_STATUS_ACTIVE;
	td[0].token = TD_TOKEN(TD_PID_SETUP, 0, 0, 0, 0x7);
	td[0].buffer = (uint32_t)setup_phys;

	td[1].link = TD_LINK_TERMINATE;
	td[1].status = TD_STATUS_ERRCNT(3) | TD_STATUS_ACTIVE;
	td[1].token = TD_TOKEN(TD_PID_IN, 0, 0, 1, 0x7FF);
	td[1].buffer = 0;

	printk(KERN_INFO "[uhci] set_addr: td_phys=0x%llx td[0].link=0x%x td[1] addr=0x%llx\n",
	       td_phys, td[0].link, td_phys + sizeof(struct uhci_td));

	uint64_t qh_phys = pmm_alloc_page();
	struct uhci_qh *qh = (struct uhci_qh *)phys_to_virt(qh_phys);
	qh->head_link = TD_LINK_TERMINATE;
	qh->element_link = (uint32_t)td_phys;

	for (int f = 0; f < FRAME_LIST_SIZE; f++)
		hcd->frame_list_virt[f] = (uint32_t)qh_phys | TD_LINK_QH;

	if (uhci_wait_td(&td[0], "SET_ADDRESS td0") < 0)
		return -1;
	if (uhci_wait_td(&td[1], "SET_ADDRESS td1") < 0)
		return -1;

	if (td[1].status & TD_STATUS_STALLED)
	{
		printk(KERN_ERR "[uhci] SET_ADDRESS stalled! status=0x%x\n", td[1].status);
		return -1;
	}

	printk(KERN_OK "[uhci] SET_ADDRESS ok, addr=%d\n", new_addr);
	return 0;
}

/* ────────────────────────────────────────────────────────
 * GET_DESCRIPTOR (Device Descriptor, 18 байт)
 * ──────────────────────────────────────────────────────── */
#define USB_DESC_DEVICE 0x01
#define USB_DEVICE_DESC_SIZE 18

struct usb_device_descriptor
{
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint16_t bcdUSB;
	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t iManufacturer;
	uint8_t iProduct;
	uint8_t iSerialNumber;
	uint8_t bNumConfigurations;
} __attribute__((packed));

static int uhci_get_device_descriptor(struct uhci_hcd *hcd,
				      uint8_t dev_addr,
				      int is_low_speed)
{
	/* setup packet */
	uint64_t setup_phys = pmm_alloc_page();
	struct usb_setup_packet *pkt = (struct usb_setup_packet *)phys_to_virt(setup_phys);
	pkt->bmRequestType = 0x80; // Device→Host, Standard, Device
	pkt->bRequest = 0x06;	   // GET_DESCRIPTOR
	pkt->wValue = (USB_DESC_DEVICE << 8) | 0x00;
	pkt->wIndex = 0;
	pkt->wLength = USB_DEVICE_DESC_SIZE;

	/* буфер для відповіді */
	uint64_t buf_phys = pmm_alloc_page();
	struct usb_device_descriptor *desc = (struct usb_device_descriptor *)phys_to_virt(buf_phys);

	uint32_t ls = is_low_speed ? TD_STATUS_LS : 0;

	/* TD */
	uint64_t td_phys = pmm_alloc_page();
	struct uhci_td *td = (struct uhci_td *)phys_to_virt(td_phys);

	/* td[0]: SETUP */
	td[0].link = (uint32_t)(td_phys + sizeof(struct uhci_td)) | TD_LINK_DEPTH;
	td[0].status = TD_STATUS_ERRCNT(3) | TD_STATUS_ACTIVE | ls;
	td[0].token = TD_TOKEN(TD_PID_SETUP, dev_addr, 0, 0, 7);
	td[0].buffer = (uint32_t)setup_phys;

	/* td[1]: IN — читаємо 18 байт дескриптора */
	td[1].link = (uint32_t)(td_phys + 2 * sizeof(struct uhci_td)) | TD_LINK_DEPTH;
	td[1].status = TD_STATUS_ERRCNT(3) | TD_STATUS_ACTIVE | ls;
	td[1].token = TD_TOKEN(TD_PID_IN, dev_addr, 0, 1, USB_DEVICE_DESC_SIZE - 1);
	td[1].buffer = (uint32_t)buf_phys;

	/* td[2]: STATUS — OUT, нульова довжина, toggle=1 */
	td[2].link = TD_LINK_TERMINATE;
	td[2].status = TD_STATUS_ERRCNT(3) | TD_STATUS_ACTIVE | ls;
	td[2].token = TD_TOKEN(TD_PID_OUT, dev_addr, 0, 1, 0x7FF);
	td[2].buffer = 0;

	/* QH */
	uint64_t qh_phys = pmm_alloc_page();
	struct uhci_qh *qh = (struct uhci_qh *)phys_to_virt(qh_phys);
	qh->head_link = TD_LINK_TERMINATE;
	qh->element_link = (uint32_t)td_phys;

	printk(KERN_INFO "[uhci] get_desc: is_low_speed=%d ls=0x%x\n", is_low_speed, ls);
	printk(KERN_INFO "[uhci] td0 status=0x%x token=0x%x\n", td[0].status, td[0].token);

	for (int f = 0; f < FRAME_LIST_SIZE; f++)
		hcd->frame_list_virt[f] = (uint32_t)qh_phys | TD_LINK_QH;

	if (uhci_wait_td(&td[0], "GET_DESC td0") < 0)
		return -1;
	if (uhci_wait_td(&td[1], "GET_DESC td1") < 0)
		return -1;
	if (uhci_wait_td(&td[2], "GET_DESC td2") < 0)
		return -1;

	if (td[1].status & TD_STATUS_STALLED)
	{
		printk(KERN_ERR "[uhci] GET_DESCRIPTOR stalled! status=0x%x\n", td[1].status);
		return -1;
	}

	printk(KERN_INFO "[uhci] Device Descriptor:\n");
	printk(KERN_INFO "  bLength=%d bDescriptorType=%d\n",
	       desc->bLength, desc->bDescriptorType);
	printk(KERN_INFO "  bcdUSB=0x%04x bDeviceClass=%d\n",
	       desc->bcdUSB, desc->bDeviceClass);
	printk(KERN_INFO "  bMaxPacketSize0=%d\n", desc->bMaxPacketSize0);
	printk(KERN_INFO "  idVendor=0x%04x idProduct=0x%04x\n",
	       desc->idVendor, desc->idProduct);
	printk(KERN_INFO "  bNumConfigurations=%d\n", desc->bNumConfigurations);

	return 0;
}

/* ────────────────────────────────────────────────────────
 * SET_CONFIGURATION
 * ──────────────────────────────────────────────────────── */
static int uhci_set_configuration(struct uhci_hcd *hcd, uint8_t dev_addr, uint8_t config)
{
	uint64_t setup_phys = pmm_alloc_page();
	struct usb_setup_packet *pkt = (struct usb_setup_packet *)phys_to_virt(setup_phys);

	pkt->bmRequestType = 0x00;
	pkt->bRequest = 0x09;
	pkt->wValue = config;
	pkt->wIndex = 0;
	pkt->wLength = 0;

	uint64_t td_phys = pmm_alloc_page();
	struct uhci_td *td = (struct uhci_td *)phys_to_virt(td_phys);

	td[0].link = (uint32_t)(td_phys + sizeof(struct uhci_td)) | TD_LINK_DEPTH;
	td[0].status = TD_STATUS_ERRCNT(3) | TD_STATUS_ACTIVE;
	td[0].token = TD_TOKEN(TD_PID_SETUP, dev_addr, 0, 0, 0x7);
	td[0].buffer = (uint32_t)setup_phys;

	td[1].link = TD_LINK_TERMINATE;
	td[1].status = TD_STATUS_ERRCNT(3) | TD_STATUS_ACTIVE;
	td[1].token = TD_TOKEN(TD_PID_IN, dev_addr, 0, 1, 0x7FF);
	td[1].buffer = 0;

	uint64_t qh_phys = pmm_alloc_page();
	struct uhci_qh *qh = (struct uhci_qh *)phys_to_virt(qh_phys);
	qh->head_link = TD_LINK_TERMINATE;
	qh->element_link = (uint32_t)td_phys;

	for (int f = 0; f < FRAME_LIST_SIZE; f++)
		hcd->frame_list_virt[f] = (uint32_t)qh_phys | TD_LINK_QH;

	if (uhci_wait_td(&td[0], "SET_ADDRESS td0") < 0)
		return -1;
	if (uhci_wait_td(&td[1], "SET_ADDRESS td1") < 0)
		return -1;

	if (td[1].status & TD_STATUS_STALLED)
	{
		printk(KERN_ERR "[uhci] SET_CONFIGURATION stalled! status=0x%x\n", td[1].status);
		return -1;
	}

	printk(KERN_OK "[uhci] SET_CONFIGURATION ok, config=%d\n", config);
	return 0;
}

/* ────────────────────────────────────────────────────────
 * Interrupt Transfer (миша)
 * ──────────────────────────────────────────────────────── */
#define MOUSE_REPORT_SIZE 4

static int uhci_interrupt_transfer(struct uhci_hcd *hcd, uint8_t dev_addr)
{
	uint64_t buf_phys = pmm_alloc_page();
	uint8_t *buf = (uint8_t *)phys_to_virt(buf_phys);

	uint64_t td_phys = pmm_alloc_page();
	struct uhci_td *td = (struct uhci_td *)phys_to_virt(td_phys);

	uint64_t qh_phys = pmm_alloc_page();
	struct uhci_qh *qh = (struct uhci_qh *)phys_to_virt(qh_phys);
	qh->head_link = TD_LINK_TERMINATE;
	qh->element_link = (uint32_t)td_phys;

	for (int i = 0; i < FRAME_LIST_SIZE; i++)
		hcd->frame_list_virt[i] = (uint32_t)qh_phys | TD_LINK_QH;

	uint8_t toggle = 0;

	while (1)
	{
		td->link = TD_LINK_TERMINATE;
		td->status = TD_STATUS_ERRCNT(3) | TD_STATUS_ACTIVE;
		td->token = TD_TOKEN(TD_PID_IN, dev_addr, 1, toggle,
				     MOUSE_REPORT_SIZE - 1);
		td->buffer = (uint32_t)buf_phys;

		qh->element_link = (uint32_t)td_phys;

		int timeout = 100000;
		while ((td->status & TD_STATUS_ACTIVE) && timeout--)
			cpu_relax();
		if (timeout == 0)
			printk(KERN_WARNING "[uhci] WARNING: TD timed out\n");

		if (td->status & TD_STATUS_STALLED)
		{
			printk(KERN_ERR "[uhci] interrupt stalled! status=0x%x\n", td->status);
			break;
		}

		printk(KERN_INFO "[uhci] mouse: %02x %02x %02x %02x\n",
		       buf[0], buf[1], buf[2], buf[3]);

		uint8_t buttons = buf[0];
		int8_t dx = (int8_t)buf[1];
		int8_t dy = (int8_t)buf[2];

		if (buttons & (1 << 0))
			printk(KERN_INFO "[uhci] LEFT\n");
		if (buttons & (1 << 1))
			printk(KERN_INFO "[uhci] RIGHT\n");
		if (buttons & (1 << 2))
			printk(KERN_INFO "[uhci] MIDDLE\n");
		if (dx)
			printk(KERN_INFO "[uhci] dx=%d\n", dx);
		if (dy)
			printk(KERN_INFO "[uhci] dy=%d\n", dy);

		toggle ^= 1;
	}

	return 0;
}

/* ────────────────────────────────────────────────────────
 * Probe
 * ──────────────────────────────────────────────────────── */
static int uhci_probe(struct pci_device *pci_dev)
{
	struct uhci_hcd *hcd = (struct uhci_hcd *)pmm_alloc_page();
	if (!hcd)
		return -1;

	uint16_t io_base = 0;
	for (int i = 0; i < 6; i++)
	{
		uint32_t bar = pci_read_config(pci_dev->bus, pci_dev->slot,
					       pci_dev->func,
					       PCI_BAR0 + i * sizeof(uint32_t));
		if (bar & PCI_BAR_IO)
		{
			io_base = (uint16_t)(bar & PCI_BAR_IO_MASK);
			printk(KERN_INFO "[uhci] I/O BAR%d = 0x%x\n", i, io_base);
			break;
		}
	}

	if (!io_base)
		return -1;

	pci_set_command(pci_dev, PCI_COMMAND_IO | PCI_COMMAND_MASTER);

	hcd->io_base = io_base;

	uhci_reset_controller(io_base);
	outw(io_base + USBSTS, 0x3F);
	uhci_init_frame_list(hcd); // ← спочатку frame list
	uhci_enable_interrupts(io_base);
	uhci_start_controller(io_base); // ← потім старт

	hcd->num_ports = uhci_count_ports(io_base);

	uhci_root_hub_poll(hcd);
	return 0;
}

/* ────────────────────────────────────────────────────────
 * Driver registration
 * ──────────────────────────────────────────────────────── */
static const struct pci_device_id uhci_ids[] = {
    {0x8086, 0x7020},
    {0x8086, 0x7112},
    {0, 0},
};

static struct pci_driver uhci_driver = {
    .name = "uhci",
    .id_table = uhci_ids,
    .probe = uhci_probe,
};

__init int uhci_module_init(void)
{
	printk(KERN_INFO "[uhci] module init\n");
	return pci_register_driver(&uhci_driver);
}

// device_initcall(uhci_module_init);
