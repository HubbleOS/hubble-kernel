/**
 * @file usb-mouse.c
 * @brief USB HID boot-protocol mouse — turns boot reports into relative
 *        motion, wheel and button events
 *
 * A boot report is: buttons, X delta, Y delta (signed bytes), and on
 * most mice a wheel delta as a fourth byte.
 */
#include <drivers/usb/usb.h>
#include <hubble/input.h>
#include <hubble/printk.h>
#include <mm/kmalloc.h>

#define HID_REQ_SET_IDLE 0x0A
#define HID_REQ_SET_PROTOCOL 0x0B
#define HID_PROTOCOL_BOOT 0

#define MOUSE_REPORT_MIN 3
#define MOUSE_BUTTONS 3

static const uint16_t button_codes[MOUSE_BUTTONS] = {BTN_LEFT, BTN_RIGHT,
                                                     BTN_MIDDLE};

struct usb_mouse {
  input_dev_t input;
  uint8_t buttons;
};

static void usb_mouse_report(struct usb_device *dev, void *ctx,
                             const uint8_t *report, int len) {
  (void)dev;
  struct usb_mouse *mouse = ctx;
  if (len < MOUSE_REPORT_MIN)
    return;

  int8_t dx = (int8_t)report[1];
  int8_t dy = (int8_t)report[2];
  int8_t wheel = len > MOUSE_REPORT_MIN ? (int8_t)report[3] : 0;
  uint8_t changed = report[0] ^ mouse->buttons;
  mouse->buttons = report[0];

  if (!dx && !dy && !wheel && !(changed & 0x07))
    return; /* nothing to report: no empty batch */

  if (dx)
    input_event(&mouse->input, EV_REL, REL_X, dx);
  if (dy)
    input_event(&mouse->input, EV_REL, REL_Y, dy);
  if (wheel)
    input_event(&mouse->input, EV_REL, REL_WHEEL, wheel);
  for (int i = 0; i < MOUSE_BUTTONS; i++)
    if (changed & (1 << i))
      input_event(&mouse->input, EV_KEY, button_codes[i], (report[0] >> i) & 1);

  input_sync(&mouse->input);
}

static void usb_mouse_disconnect(struct usb_device *dev, void *data) {
  (void)dev;
  struct usb_mouse *mouse = data;
  input_unregister_device(&mouse->input);
  kfree(mouse);
}

int usb_mouse_probe(struct usb_device *dev,
                    const struct usb_interface_descriptor *iface,
                    const struct usb_endpoint_descriptor *ep) {
  uint8_t type = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE;

  if (usb_control_msg(dev, type, HID_REQ_SET_PROTOCOL, HID_PROTOCOL_BOOT,
                      iface->bInterfaceNumber, NULL, 0) < 0) {
    printk(KERN_ERR "[usb-mouse] SET_PROTOCOL(boot) failed\n");
    return -1;
  }

  /* Report only on change. Optional: many mice stall it. */
  usb_control_msg(dev, type, HID_REQ_SET_IDLE, 0, iface->bInterfaceNumber, NULL,
                  0);

  struct usb_mouse *mouse = kzalloc(sizeof(*mouse));
  if (!mouse)
    return -1;

  mouse->input.name = "usb-mouse";
  input_set_bit(EV_KEY, mouse->input.evbit);
  input_set_bit(EV_REL, mouse->input.evbit);
  input_register_device(&mouse->input);

  if (dev->ops->interrupt_in(dev, ep, usb_mouse_report, mouse) < 0) {
    printk(KERN_ERR "[usb-mouse] failed to start the report endpoint\n");
    usb_mouse_disconnect(dev, mouse);
    return -1;
  }
  usb_bind(dev, mouse, usb_mouse_disconnect);

  printk(KERN_OK "[usb-mouse] mouse ready on port %d\n", dev->port);
  return 0;
}
