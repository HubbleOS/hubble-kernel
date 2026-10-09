/**
 * @file usb.c
 * @brief USB core — device enumeration after addressing, configuration
 *        selection and class driver binding
 */
#include <drivers/usb/usb.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <stdbool.h>
#include <stddef.h>

/* Large enough for any realistic boot-device configuration. */
#define USB_CONFIG_MAX 1024

int usb_control_msg(struct usb_device *dev, uint8_t request_type,
                    uint8_t request, uint16_t value, uint16_t index, void *data,
                    uint16_t length) {
  struct usb_setup_packet setup = {
      .bmRequestType = request_type,
      .bRequest = request,
      .wValue = value,
      .wIndex = index,
      .wLength = length,
  };
  return dev->ops->control(dev, &setup, data);
}

static int usb_get_descriptor(struct usb_device *dev, uint8_t type, void *buf,
                              uint16_t length) {
  return usb_control_msg(dev, USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                         USB_REQ_GET_DESCRIPTOR, (uint16_t)(type << 8), 0, buf,
                         length);
}

/* -- Class driver binding --------------------------------- */

static int usb_probe_interface(struct usb_device *dev,
                               const struct usb_interface_descriptor *iface,
                               const struct usb_endpoint_descriptor *ep) {
  if (dev->nbindings == USB_MAX_BINDINGS ||
      iface->bInterfaceClass != USB_CLASS_HID ||
      iface->bInterfaceSubClass != USB_SUBCLASS_BOOT)
    return -1;

  switch (iface->bInterfaceProtocol) {
  case USB_PROTOCOL_KEYBOARD:
    return usb_kbd_probe(dev, iface, ep);
  case USB_PROTOCOL_MOUSE:
    return usb_mouse_probe(dev, iface, ep);
  default:
    return -1;
  }
}

/**
 * Offer every interface (default alternate setting) with an interrupt IN
 * endpoint to the class drivers: a wireless receiver is a keyboard and a
 * mouse at once. Only HID boot devices are handled for now. Returns the
 * number of interfaces bound.
 */
static int usb_bind_drivers(struct usb_device *dev, const uint8_t *cfg,
                            uint16_t total) {
  const struct usb_interface_descriptor *iface = NULL;
  int bound = 0;

  for (uint16_t off = 0; off + 2 <= total && cfg[off] >= 2; off += cfg[off]) {
    if (off + cfg[off] > total)
      break;

    if (cfg[off + 1] == USB_DESC_INTERFACE &&
        cfg[off] >= sizeof(struct usb_interface_descriptor)) {
      iface = (const struct usb_interface_descriptor *)&cfg[off];
      if (iface->bAlternateSetting != 0)
        iface = NULL;
      continue;
    }

    if (cfg[off + 1] != USB_DESC_ENDPOINT || !iface ||
        cfg[off] < sizeof(struct usb_endpoint_descriptor))
      continue;

    const struct usb_endpoint_descriptor *ep =
        (const struct usb_endpoint_descriptor *)&cfg[off];
    bool int_in =
        (ep->bEndpointAddress & USB_ENDPOINT_DIR_IN) &&
        (ep->bmAttributes & USB_ENDPOINT_XFER_MASK) == USB_ENDPOINT_XFER_INT;
    if (!int_in)
      continue;

    if (usb_probe_interface(dev, iface, ep) == 0)
      bound++;
    iface = NULL; /* one report endpoint per interface */
  }

  return bound;
}

int usb_bind(struct usb_device *dev, void *data,
             void (*disconnect)(struct usb_device *dev, void *data)) {
  if (dev->nbindings == USB_MAX_BINDINGS)
    return -1;
  dev->bindings[dev->nbindings++] = (struct usb_binding){data, disconnect};
  return 0;
}

/* -- Enumeration ------------------------------------------ */

int usb_probe_device(struct usb_device *dev) {
  if (usb_get_descriptor(dev, USB_DESC_DEVICE, &dev->desc, sizeof(dev->desc)) <
      0) {
    printk(KERN_ERR "[usb] port %d: GET_DESCRIPTOR(device) failed\n",
           dev->port);
    return -1;
  }

  printk(KERN_INFO "[usb] port %d: device %04x:%04x class %02x, usb %x.%02x\n",
         dev->port, dev->desc.idVendor, dev->desc.idProduct,
         dev->desc.bDeviceClass, dev->desc.bcdUSB >> 8,
         dev->desc.bcdUSB & 0xFF);

  struct usb_config_descriptor head;
  if (usb_get_descriptor(dev, USB_DESC_CONFIGURATION, &head, sizeof(head)) <
      0) {
    printk(KERN_ERR "[usb] port %d: GET_DESCRIPTOR(config) failed\n",
           dev->port);
    return -1;
  }

  uint16_t total = head.wTotalLength;
  if (total < sizeof(head) || total > USB_CONFIG_MAX) {
    printk(KERN_ERR "[usb] port %d: bad config length %d\n", dev->port, total);
    return -1;
  }

  uint8_t *cfg = kmalloc(total, GFP_KERNEL);
  if (!cfg)
    return -1;

  int ret = -1;
  if (usb_get_descriptor(dev, USB_DESC_CONFIGURATION, cfg, total) < 0) {
    printk(KERN_ERR "[usb] port %d: reading config failed\n", dev->port);
    goto out;
  }

  if (usb_control_msg(dev, USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                      USB_REQ_SET_CONFIGURATION, head.bConfigurationValue, 0,
                      NULL, 0) < 0) {
    printk(KERN_ERR "[usb] port %d: SET_CONFIGURATION failed\n", dev->port);
    goto out;
  }

  if (usb_bind_drivers(dev, cfg, total) > 0)
    ret = 0;
  else
    printk(KERN_INFO "[usb] port %d: no driver for this device\n", dev->port);

out:
  kfree(cfg);
  return ret;
}

void usb_disconnect_device(struct usb_device *dev) {
  for (int i = 0; i < dev->nbindings; i++)
    dev->bindings[i].disconnect(dev, dev->bindings[i].data);
  dev->nbindings = 0;
}
