/**
 * @file usb-kbd.c
 * @brief USB HID boot-protocol keyboard — turns 8-byte boot reports into
 *        input core key events
 *
 * Key codes are reported as PS/2 set-1 scancodes (0x100 marks an E0
 * extended key), the same codes the PS/2 driver reports, so the tty
 * keymap serves both keyboards.
 */
#include <drivers/usb/usb.h>
#include <hubble/input.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <stdbool.h>

#define HID_REQ_SET_IDLE 0x0A
#define HID_REQ_SET_PROTOCOL 0x0B
#define HID_PROTOCOL_BOOT 0

#define KBD_REPORT_SIZE 8
#define KBD_KEY_SLOTS 6
#define HID_USAGE_ROLLOVER 0x01

#define EXT 0x100

/* -- HID usage (keyboard page) -> set-1 scancode ---------- */

static const uint16_t hid_to_set1[256] = {
    [0x04] = 0x1E,       [0x05] = 0x30, [0x06] = 0x2E,
    [0x07] = 0x20, /* a-d */
    [0x08] = 0x12,       [0x09] = 0x21, [0x0A] = 0x22,
    [0x0B] = 0x23, /* e-h */
    [0x0C] = 0x17,       [0x0D] = 0x24, [0x0E] = 0x25,
    [0x0F] = 0x26, /* i-l */
    [0x10] = 0x32,       [0x11] = 0x31, [0x12] = 0x18,
    [0x13] = 0x19, /* m-p */
    [0x14] = 0x10,       [0x15] = 0x13, [0x16] = 0x1F,
    [0x17] = 0x14, /* q-t */
    [0x18] = 0x16,       [0x19] = 0x2F, [0x1A] = 0x11,
    [0x1B] = 0x2D,                      /* u-x */
    [0x1C] = 0x15,       [0x1D] = 0x2C, /* y-z */
    [0x1E] = 0x02,       [0x1F] = 0x03, [0x20] = 0x04,
    [0x21] = 0x05, /* 1-4 */
    [0x22] = 0x06,       [0x23] = 0x07, [0x24] = 0x08,
    [0x25] = 0x09,                      /* 5-8 */
    [0x26] = 0x0A,       [0x27] = 0x0B, /* 9 0 */
    [0x28] = 0x1C,                      /* Enter */
    [0x29] = 0x01,                      /* Esc */
    [0x2A] = 0x0E,                      /* Backspace */
    [0x2B] = 0x0F,                      /* Tab */
    [0x2C] = 0x39,                      /* Space */
    [0x2D] = 0x0C,                      /* - */
    [0x2E] = 0x0D,                      /* = */
    [0x2F] = 0x1A,                      /* [ */
    [0x30] = 0x1B,                      /* ] */
    [0x31] = 0x2B,                      /* \ */
    [0x32] = 0x2B,                      /* non-US # */
    [0x33] = 0x27,                      /* ; */
    [0x34] = 0x28,                      /* ' */
    [0x35] = 0x29,                      /* ` */
    [0x36] = 0x33,                      /* , */
    [0x37] = 0x34,                      /* . */
    [0x38] = 0x35,                      /* / */
    [0x39] = 0x3A,                      /* Caps Lock */
    [0x3A] = 0x3B,       [0x3B] = 0x3C, [0x3C] = 0x3D,
    [0x3D] = 0x3E, /* F1-F4 */
    [0x3E] = 0x3F,       [0x3F] = 0x40, [0x40] = 0x41,
    [0x41] = 0x42, /* F5-F8 */
    [0x42] = 0x43,       [0x43] = 0x44, [0x44] = 0x57,
    [0x45] = 0x58,                                     /* F9-F12 */
    [0x46] = EXT | 0x37,                               /* Print Screen */
    [0x47] = 0x46,                                     /* Scroll Lock */
    [0x49] = EXT | 0x52,                               /* Insert */
    [0x4A] = EXT | 0x47,                               /* Home */
    [0x4B] = EXT | 0x49,                               /* Page Up */
    [0x4C] = EXT | 0x53,                               /* Delete */
    [0x4D] = EXT | 0x4F,                               /* End */
    [0x4E] = EXT | 0x51,                               /* Page Down */
    [0x4F] = EXT | 0x4D,                               /* Right */
    [0x50] = EXT | 0x4B,                               /* Left */
    [0x51] = EXT | 0x50,                               /* Down */
    [0x52] = EXT | 0x48,                               /* Up */
    [0x53] = 0x45,                                     /* Num Lock */
    [0x54] = EXT | 0x35,                               /* KP / */
    [0x55] = 0x37,                                     /* KP * */
    [0x56] = 0x4A,                                     /* KP - */
    [0x57] = 0x4E,                                     /* KP + */
    [0x58] = EXT | 0x1C,                               /* KP Enter */
    [0x59] = 0x4F,       [0x5A] = 0x50, [0x5B] = 0x51, /* KP 1-3 */
    [0x5C] = 0x4B,       [0x5D] = 0x4C, [0x5E] = 0x4D, /* KP 4-6 */
    [0x5F] = 0x47,       [0x60] = 0x48, [0x61] = 0x49, /* KP 7-9 */
    [0x62] = 0x52,                                     /* KP 0 */
    [0x63] = 0x53,                                     /* KP . */
    [0x64] = 0x56,                                     /* non-US \ */
    [0x65] = EXT | 0x5D,                               /* Menu */
};

/* Modifier byte, bit 0 (LCtrl) .. bit 7 (RGUI) */
static const uint16_t modifier_to_set1[8] = {
    0x1D,       /* Left Ctrl */
    0x2A,       /* Left Shift */
    0x38,       /* Left Alt */
    EXT | 0x5B, /* Left GUI */
    EXT | 0x1D, /* Right Ctrl */
    0x36,       /* Right Shift */
    EXT | 0x38, /* Right Alt */
    EXT | 0x5C, /* Right GUI */
};

/* -- Per-keyboard state ----------------------------------- */

struct usb_kbd {
  input_dev_t input;
  uint8_t last[KBD_REPORT_SIZE];
};

static void usb_kbd_key(struct usb_kbd *kbd, uint16_t code, bool pressed) {
  if (!code)
    return;
  input_raw_event_t ev = {
      .type = EV_KEY,
      .code = code,
      .value = pressed ? 1 : 0,
  };
  input_report(&kbd->input, &ev);
}

static bool report_has_key(const uint8_t *report, uint8_t usage) {
  for (int i = 2; i < 2 + KBD_KEY_SLOTS; i++)
    if (report[i] == usage)
      return true;
  return false;
}

/* -- Report handling -------------------------------------- */

/**
 * A boot report is the full keyboard state: diff it against the previous
 * one to get press and release events.
 */
static void usb_kbd_report(struct usb_device *dev, const uint8_t *report,
                           int len) {
  struct usb_kbd *kbd = dev->driver_data;
  if (!kbd || len < KBD_REPORT_SIZE)
    return;

  /* Too many keys down: the device reports "rollover" and no state. */
  if (report[2] == HID_USAGE_ROLLOVER)
    return;

  uint8_t changed = report[0] ^ kbd->last[0];
  for (int bit = 0; bit < 8; bit++)
    if (changed & (1 << bit))
      usb_kbd_key(kbd, modifier_to_set1[bit], report[0] & (1 << bit));

  for (int i = 2; i < 2 + KBD_KEY_SLOTS; i++) {
    uint8_t usage = kbd->last[i];
    if (usage > HID_USAGE_ROLLOVER && !report_has_key(report, usage))
      usb_kbd_key(kbd, hid_to_set1[usage], false);
  }

  for (int i = 2; i < 2 + KBD_KEY_SLOTS; i++) {
    uint8_t usage = report[i];
    if (usage > HID_USAGE_ROLLOVER && !report_has_key(kbd->last, usage))
      usb_kbd_key(kbd, hid_to_set1[usage], true);
  }

  memcpy(kbd->last, report, KBD_REPORT_SIZE);
}

static void usb_kbd_disconnect(struct usb_device *dev) {
  struct usb_kbd *kbd = dev->driver_data;
  input_unregister_device(&kbd->input);
  kfree(kbd);
}

/* -- Probe ------------------------------------------------ */

int usb_kbd_probe(struct usb_device *dev,
                  const struct usb_interface_descriptor *iface,
                  const struct usb_endpoint_descriptor *ep) {
  uint8_t type = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE;

  if (usb_control_msg(dev, type, HID_REQ_SET_PROTOCOL, HID_PROTOCOL_BOOT,
                      iface->bInterfaceNumber, NULL, 0) < 0) {
    printk(KERN_ERR "[usb-kbd] SET_PROTOCOL(boot) failed\n");
    return -1;
  }

  /* Report only on change. Optional: many keyboards stall it. */
  usb_control_msg(dev, type, HID_REQ_SET_IDLE, 0, iface->bInterfaceNumber, NULL,
                  0);

  struct usb_kbd *kbd = kzalloc(sizeof(*kbd));
  if (!kbd)
    return -1;

  kbd->input.name = "usb-keyboard";
  input_set_bit(EV_KEY, kbd->input.evbit);

  dev->driver_data = kbd;
  dev->disconnect = usb_kbd_disconnect;
  input_register_device(&kbd->input);

  if (dev->ops->interrupt_in(dev, ep, usb_kbd_report) < 0) {
    printk(KERN_ERR "[usb-kbd] failed to start the report endpoint\n");
    usb_disconnect_device(dev);
    return -1;
  }

  printk(KERN_OK "[usb-kbd] keyboard ready on port %d\n", dev->port);
  return 0;
}
