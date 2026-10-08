/**
 * @file usb.h
 * @brief USB core — standard descriptors, requests, and the interface
 *        between host controller drivers and class drivers
 *
 * A host controller driver (xHCI, ...) brings a device up to the Address
 * state, fills in a usb_device and hands it to usb_probe_device(). The core
 * reads the descriptors, selects the configuration and binds a class
 * driver, which then talks to the device through usb_control_msg() and
 * the controller's interrupt_in().
 */
#pragma once

#include <stdint.h>

/* -- Standard requests and descriptor types -------------- */

#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_REQ_SET_CONFIGURATION 0x09

#define USB_DIR_OUT 0x00
#define USB_DIR_IN 0x80
#define USB_TYPE_STANDARD 0x00
#define USB_TYPE_CLASS 0x20
#define USB_RECIP_DEVICE 0x00
#define USB_RECIP_INTERFACE 0x01

#define USB_DESC_DEVICE 0x01
#define USB_DESC_CONFIGURATION 0x02
#define USB_DESC_INTERFACE 0x04
#define USB_DESC_ENDPOINT 0x05

#define USB_ENDPOINT_DIR_IN 0x80
#define USB_ENDPOINT_NUMBER_MASK 0x0F
#define USB_ENDPOINT_XFER_MASK 0x03
#define USB_ENDPOINT_XFER_INT 0x03

#define USB_CLASS_HID 0x03
#define USB_SUBCLASS_BOOT 0x01
#define USB_PROTOCOL_KEYBOARD 0x01
#define USB_PROTOCOL_MOUSE 0x02

/* -- Device speeds (xHCI PORTSC numbering) --------------- */

#define USB_SPEED_FULL 1
#define USB_SPEED_LOW 2
#define USB_SPEED_HIGH 3
#define USB_SPEED_SUPER 4

/* -- Wire formats ----------------------------------------- */

/** @brief Standard USB setup packet for control transfers */
struct usb_setup_packet {
  uint8_t bmRequestType;
  uint8_t bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
} __attribute__((packed));

struct usb_device_descriptor {
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

struct usb_config_descriptor {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint16_t wTotalLength;
  uint8_t bNumInterfaces;
  uint8_t bConfigurationValue;
  uint8_t iConfiguration;
  uint8_t bmAttributes;
  uint8_t bMaxPower;
} __attribute__((packed));

struct usb_interface_descriptor {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bInterfaceNumber;
  uint8_t bAlternateSetting;
  uint8_t bNumEndpoints;
  uint8_t bInterfaceClass;
  uint8_t bInterfaceSubClass;
  uint8_t bInterfaceProtocol;
  uint8_t iInterface;
} __attribute__((packed));

struct usb_endpoint_descriptor {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bEndpointAddress;
  uint8_t bmAttributes;
  uint16_t wMaxPacketSize;
  uint8_t bInterval;
} __attribute__((packed));

/* -- Core device model ------------------------------------ */

struct usb_device;

/** @brief Called with each completed interrupt IN transfer */
typedef void (*usb_complete_t)(struct usb_device *dev, const uint8_t *data,
                               int len);

/** @brief Operations a host controller driver provides for its devices */
struct usb_hcd_ops {
  /**
   * Run one control transfer to completion. @p data holds setup->wLength
   * bytes (sent for OUT, filled for IN). Returns 0 or a negative error.
   */
  int (*control)(struct usb_device *dev, const struct usb_setup_packet *setup,
                 void *data);
  /**
   * Enable an interrupt IN endpoint and keep it polled: @p complete runs
   * for every report until the device goes away.
   */
  int (*interrupt_in)(struct usb_device *dev,
                      const struct usb_endpoint_descriptor *ep,
                      usb_complete_t complete);
};

/** @brief One addressed USB device, owned by its host controller driver */
struct usb_device {
  const struct usb_hcd_ops *ops;
  void *hcd_priv; /**< Host controller's per-device state */
  uint8_t speed;  /**< USB_SPEED_* */
  uint8_t port;   /**< Root hub port, 1-based */
  struct usb_device_descriptor desc;
  void *driver_data;                          /**< Bound class driver's state */
  void (*disconnect)(struct usb_device *dev); /**< Set by the class driver */
};

/* -- Core API --------------------------------------------- */

int usb_control_msg(struct usb_device *dev, uint8_t request_type,
                    uint8_t request, uint16_t value, uint16_t index, void *data,
                    uint16_t length);

/** Read descriptors, set the configuration and bind a class driver. */
int usb_probe_device(struct usb_device *dev);

/** Detach the bound class driver; the HCD frees the device afterwards. */
void usb_disconnect_device(struct usb_device *dev);

/* -- Class drivers ---------------------------------------- */

int usb_kbd_probe(struct usb_device *dev,
                  const struct usb_interface_descriptor *iface,
                  const struct usb_endpoint_descriptor *ep);
