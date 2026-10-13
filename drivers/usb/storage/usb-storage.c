/**
 * @file usb-storage.c
 * @brief USB mass-storage driver (stub — not yet implemented)
 */
#include "usb-storage.h"
#include <drivers/usb/usb.h>
#include <hubble/printk.h>
#include <mm/kmalloc.h>

typedef struct {
  struct usb_device *dev;
  uint8_t bulk_in;
  uint8_t bulk_out;
} usb_storage_t;
#define USB_ENDPOINT_XFERTYPE_MASK 0x03
#define USB_ENDPOINT_XFER_BULK 0x02

int usb_storage_probe(struct usb_device *dev,
                      const struct usb_interface_descriptor *iface,
                      const struct usb_endpoint_descriptor *ep) {
  if (iface->bInterfaceClass != USB_CLASS_MASS_STORAGE ||
      iface->bInterfaceSubClass != USB_SUBCLASS_SCSI ||
      iface->bInterfaceProtocol != USB_PROTOCOL_BULK_ONLY)
    return -1;

  usb_storage_t *storage = kmalloc(sizeof(usb_storage_t), GFP_KERNEL);
  if (!storage)
    return -1;
	
  storage->dev = dev;
  storage->bulk_in = 0;
  storage->bulk_out = 0;

  for (int i = 0; i < iface->bNumEndpoints; i++) {
    const struct usb_endpoint_descriptor *e =
        (const struct usb_endpoint_descriptor *)(ep + i);
    if ((e->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
        USB_ENDPOINT_XFER_BULK) {
      if (e->bEndpointAddress & USB_DIR_IN)
        storage->bulk_in = e->bEndpointAddress;
      else
        storage->bulk_out = e->bEndpointAddress;
    }
  }

  if (!storage->bulk_in || !storage->bulk_out) {
    kfree(storage);
    return -1;
  }

  return usb_bind(dev, storage, NULL);
}
