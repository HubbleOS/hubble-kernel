/**
 * @file usb-mouse.c
 * @brief USB HID mouse — turns reports into relative motion, wheel and
 *        button events
 *
 * Many mice (wireless receivers especially) ignore SET_PROTOCOL(boot) and
 * keep sending their own report format: a report ID first, 12- or 16-bit
 * X/Y. So the mouse's report descriptor is read and parsed for where the
 * buttons, X, Y and wheel live. Only when that fails is the device put in
 * boot protocol, whose layout is fixed: buttons, X, Y (signed bytes) and
 * on most mice a wheel byte.
 */
#include <drivers/usb/usb.h>
#include <hubble/input.h>
#include <hubble/printk.h>
#include <mm/kmalloc.h>
#include <stdbool.h>

#define HID_REQ_SET_IDLE 0x0A
#define HID_REQ_SET_PROTOCOL 0x0B
#define HID_PROTOCOL_BOOT 0
#define HID_PROTOCOL_REPORT 1

#define HID_DESC_HID 0x21
#define HID_DESC_REPORT 0x22
#define HID_REPORT_DESC_MAX 1024

#define MOUSE_BUTTONS 3

static const uint16_t button_codes[MOUSE_BUTTONS] = {BTN_LEFT, BTN_RIGHT,
                                                     BTN_MIDDLE};

/** @brief One value in a report: bit offset and width */
struct hid_field {
  uint16_t offset; /**< Bits from the start of the report data */
  uint8_t size;    /**< Bits; 0 = not present */
  bool is_signed;
};

/** @brief Where a mouse report keeps what we need */
struct mouse_layout {
  int report_id; /**< -1 if the device uses no report IDs */
  struct hid_field x, y, wheel;
  struct hid_field buttons; /**< Button 1 at offset, one bit each */
  uint8_t nbuttons;
};

struct usb_mouse {
  input_dev_t input;
  struct mouse_layout layout;
  uint8_t buttons;
};

/* -- Report descriptor parsing ---------------------------- */

#define HID_PAGE_DESKTOP 0x01
#define HID_PAGE_BUTTON 0x09
#define HID_USAGE_X 0x30
#define HID_USAGE_Y 0x31
#define HID_USAGE_WHEEL 0x38

#define HID_INPUT_CONSTANT 0x01
#define HID_INPUT_VARIABLE 0x02
#define HID_INPUT_RELATIVE 0x04

#define HID_MAX_USAGES 16
#define HID_STACK_DEPTH 4

struct hid_globals {
  uint16_t page;
  int32_t logical_min;
  uint32_t report_size;
  uint32_t report_count;
  int report_id;
};

struct hid_parser {
  struct hid_globals g;
  struct hid_globals stack[HID_STACK_DEPTH];
  int depth;
  uint32_t usages[HID_MAX_USAGES]; /**< page << 16 | usage */
  int nusages;
  uint32_t usage_min, usage_max;
  bool have_range;
  uint32_t offset; /**< Bit offset in the current report */
  int buttons_id;  /**< Report ID the buttons were found in */
};

static uint32_t hid_item_value(const uint8_t *data, int size) {
  uint32_t v = 0;
  for (int i = 0; i < size; i++)
    v |= (uint32_t)data[i] << (8 * i);
  return v;
}

static int32_t hid_item_signed(const uint8_t *data, int size) {
  uint32_t v = hid_item_value(data, size);
  if (size && size < 4 && (v & (1u << (size * 8 - 1))))
    v |= ~0u << (size * 8);
  return (int32_t)v;
}

/** A local usage, with the usage page filled in unless it is extended. */
static uint32_t hid_full_usage(const struct hid_parser *p, uint32_t v,
                               int size) {
  return size == 4 ? v : ((uint32_t)p->g.page << 16) | v;
}

static void hid_set_field(struct hid_field *f, uint32_t offset, uint32_t size,
                          bool is_signed) {
  if (f->size || !size || size > 32)
    return; /* first match wins */
  f->offset = offset;
  f->size = size;
  f->is_signed = is_signed;
}

/** Record whatever this Input item gives us from the report ID found first. */
static void hid_input(struct hid_parser *p, uint32_t flags,
                      struct mouse_layout *l) {
  uint32_t size = p->g.report_size, count = p->g.report_count;

  if ((flags & HID_INPUT_CONSTANT) || !(flags & HID_INPUT_VARIABLE))
    return;
  if (l->x.size && p->g.report_id != l->report_id)
    return; /* another report (consumer keys, ...): not ours */

  bool is_signed = p->g.logical_min < 0;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t usage;
    if (p->have_range)
      usage = p->usage_min + i <= p->usage_max ? p->usage_min + i : 0;
    else if (p->nusages)
      usage = p->usages[i < (uint32_t)p->nusages ? i : (uint32_t)p->nusages - 1];
    else
      continue;

    uint32_t off = p->offset + i * size;
    uint16_t page = usage >> 16, id = usage & 0xFFFF;
    struct hid_field *f = NULL;

    if (page == HID_PAGE_DESKTOP && (flags & HID_INPUT_RELATIVE)) {
      f = id == HID_USAGE_X       ? &l->x
          : id == HID_USAGE_Y     ? &l->y
          : id == HID_USAGE_WHEEL ? &l->wheel
                                  : NULL;
    } else if (page == HID_PAGE_BUTTON && size == 1 && id >= 1 &&
               id <= MOUSE_BUTTONS) {
      if (id == 1 && !l->buttons.size) {
        hid_set_field(&l->buttons, off, 1, false);
        p->buttons_id = p->g.report_id;
      }
      if (l->buttons.size && p->buttons_id == p->g.report_id &&
          off == l->buttons.offset + id - 1u && id > l->nbuttons)
        l->nbuttons = id;
      continue;
    }
    if (!f)
      continue;
    if (f == &l->x && !l->x.size)
      l->report_id = p->g.report_id;
    hid_set_field(f, off, size, is_signed);
  }
}

/**
 * Find the buttons, X, Y and wheel in a report descriptor. Fields are
 * only taken from the report that carries X. Returns false if there is
 * no relative X/Y.
 */
static bool hid_parse_mouse(const uint8_t *desc, int len,
                            struct mouse_layout *l) {
  struct hid_parser p = {.g = {.report_id = -1}};
  *l = (struct mouse_layout){.report_id = -1};

  for (int pos = 0; pos < len;) {
    uint8_t prefix = desc[pos++];
    if (prefix == 0xFE) { /* long item: size byte, tag byte, data */
      if (pos >= len)
        break;
      pos += 2 + desc[pos];
      continue;
    }

    int size = (prefix & 3) == 3 ? 4 : prefix & 3;
    if (pos + size > len)
      break;
    const uint8_t *data = &desc[pos];
    pos += size;
    uint32_t v = hid_item_value(data, size);
    uint8_t type = (prefix >> 2) & 3, tag = prefix >> 4;

    if (type == 0) { /* main */
      if (tag == 0x8) { /* Input; Output and Feature are other reports */
        hid_input(&p, v, l);
        p.offset += p.g.report_size * p.g.report_count;
      }
      p.nusages = 0;
      p.have_range = false;
    } else if (type == 1) { /* global */
      switch (tag) {
      case 0x0: p.g.page = v; break;
      case 0x1: p.g.logical_min = hid_item_signed(data, size); break;
      case 0x7: p.g.report_size = v; break;
      case 0x8: /* a new report ID starts a new report */
        p.g.report_id = v;
        p.offset = 0;
        break;
      case 0x9: p.g.report_count = v; break;
      case 0xA: /* Push */
        if (p.depth < HID_STACK_DEPTH)
          p.stack[p.depth++] = p.g;
        break;
      case 0xB: /* Pop */
        if (p.depth > 0)
          p.g = p.stack[--p.depth];
        break;
      }
    } else if (type == 2) { /* local */
      switch (tag) {
      case 0x0:
        if (p.nusages < HID_MAX_USAGES)
          p.usages[p.nusages++] = hid_full_usage(&p, v, size);
        break;
      case 0x1:
        p.usage_min = hid_full_usage(&p, v, size);
        p.have_range = true;
        break;
      case 0x2: p.usage_max = hid_full_usage(&p, v, size); break;
      }
    }
  }

  if (p.buttons_id != l->report_id)
    l->buttons.size = l->nbuttons = 0; /* buttons were in another report */
  return l->x.size && l->y.size;
}

/* The fixed boot protocol report. */
static const struct mouse_layout boot_layout = {
    .report_id = -1,
    .buttons = {0, 1, false},
    .nbuttons = MOUSE_BUTTONS,
    .x = {8, 8, true},
    .y = {16, 8, true},
    .wheel = {24, 8, true},
};

/**
 * Read the interface's report descriptor; its length is in the HID
 * descriptor, which sits between the interface and endpoint descriptors.
 */
static bool usb_mouse_read_layout(struct usb_device *dev,
                                  const struct usb_interface_descriptor *iface,
                                  const struct usb_endpoint_descriptor *ep,
                                  struct mouse_layout *l) {
  const uint8_t *hid = (const uint8_t *)iface + iface->bLength;
  if (hid + 9 > (const uint8_t *)ep || hid[1] != HID_DESC_HID ||
      hid[6] != HID_DESC_REPORT)
    return false;

  uint16_t len = hid[7] | (hid[8] << 8);
  if (!len || len > HID_REPORT_DESC_MAX)
    return false;

  uint8_t *desc = kmalloc(len, GFP_KERNEL);
  if (!desc)
    return false;

  bool ok = usb_control_msg(dev,
                            USB_DIR_IN | USB_TYPE_STANDARD |
                                USB_RECIP_INTERFACE,
                            USB_REQ_GET_DESCRIPTOR, HID_DESC_REPORT << 8,
                            iface->bInterfaceNumber, desc, len) == 0 &&
            hid_parse_mouse(desc, len, l);
  kfree(desc);
  return ok;
}

/* -- Reports ---------------------------------------------- */

/** Extract a field from @p len bytes of report data; 0 if it is cut off. */
static int32_t hid_get(const uint8_t *data, int len, struct hid_field f) {
  if (!f.size || f.offset + f.size > (uint32_t)len * 8)
    return 0;

  uint64_t v = 0;
  for (uint32_t i = 0; i < f.size; i++) {
    uint32_t bit = f.offset + i;
    v |= (uint64_t)((data[bit / 8] >> (bit % 8)) & 1) << i;
  }
  if (f.is_signed && (v & (1ull << (f.size - 1))))
    v |= ~0ull << f.size;
  return (int32_t)v;
}

static void usb_mouse_report(struct usb_device *dev, void *ctx,
                             const uint8_t *report, int len) {
  (void)dev;
  struct usb_mouse *mouse = ctx;
  const struct mouse_layout *l = &mouse->layout;

  if (l->report_id >= 0) {
    if (len < 1 || report[0] != l->report_id)
      return; /* another report on this interface */
    report++;
    len--;
  }

  int32_t dx = hid_get(report, len, l->x);
  int32_t dy = hid_get(report, len, l->y);
  int32_t wheel = hid_get(report, len, l->wheel);
  uint8_t buttons = 0;
  for (int i = 0; i < l->nbuttons; i++) {
    struct hid_field b = {l->buttons.offset + i, 1, false};
    buttons |= hid_get(report, len, b) << i;
  }
  uint8_t changed = buttons ^ mouse->buttons;
  mouse->buttons = buttons;

  if (!dx && !dy && !wheel && !changed)
    return; /* nothing to report: no empty batch */

  if (dx)
    input_event(&mouse->input, EV_REL, REL_X, dx);
  if (dy)
    input_event(&mouse->input, EV_REL, REL_Y, dy);
  if (wheel)
    input_event(&mouse->input, EV_REL, REL_WHEEL, wheel);
  for (int i = 0; i < MOUSE_BUTTONS; i++)
    if (changed & (1 << i))
      input_event(&mouse->input, EV_KEY, button_codes[i], (buttons >> i) & 1);

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

  struct usb_mouse *mouse = kzalloc(sizeof(*mouse));
  if (!mouse)
    return -1;

  if (usb_mouse_read_layout(dev, iface, ep, &mouse->layout)) {
    /* Report protocol is the default; say so anyway, firmware may not. */
    usb_control_msg(dev, type, HID_REQ_SET_PROTOCOL, HID_PROTOCOL_REPORT,
                    iface->bInterfaceNumber, NULL, 0);
    printk(KERN_INFO "[usb-mouse] report id %d, x %d:%d, y %d:%d, "
                     "wheel %d:%d, %d buttons\n",
           mouse->layout.report_id, mouse->layout.x.offset,
           mouse->layout.x.size, mouse->layout.y.offset, mouse->layout.y.size,
           mouse->layout.wheel.offset, mouse->layout.wheel.size,
           mouse->layout.nbuttons);
  } else {
    if (usb_control_msg(dev, type, HID_REQ_SET_PROTOCOL, HID_PROTOCOL_BOOT,
                        iface->bInterfaceNumber, NULL, 0) < 0) {
      printk(KERN_ERR "[usb-mouse] SET_PROTOCOL(boot) failed\n");
      kfree(mouse);
      return -1;
    }
    mouse->layout = boot_layout;
  }

  /* Report only on change. Optional: many mice stall it. */
  usb_control_msg(dev, type, HID_REQ_SET_IDLE, 0, iface->bInterfaceNumber, NULL,
                  0);

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
