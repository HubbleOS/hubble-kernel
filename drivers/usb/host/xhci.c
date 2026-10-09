/**
 * @file xhci.c
 * @brief xHCI host controller driver — controller bring-up, command and
 *        event rings, root port handling, control and interrupt transfers
 *
 * Interrupts are not used: one kernel thread per controller polls the
 * event ring. That thread does all of the controller's work (port
 * changes, enumeration, report delivery), so the driver needs no locks.
 * Synchronous waits during enumeration keep dispatching unrelated events.
 */
#include "xhci.h"
#include <drivers/pci/pci.h>
#include <drivers/usb/usb.h>
#include <higher_half.h>
#include <hpet/hpet.h>
#include <hubble/module.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <smp/scheduler.h>
#include <stdbool.h>

#define cpu_relax() asm volatile("pause" ::: "memory")
#define barrier() asm volatile("" ::: "memory")

#define XHCI_RING_TRBS (VMM_PAGE_SIZE / sizeof(struct xhci_trb))
#define XHCI_TIMEOUT_NS 1000000000ULL /* commands and control transfers */
#define XHCI_POLL_NS 4000000ULL       /* event ring polling period */
#define XHCI_THREAD_PRIORITY 100
#define XHCI_MAX_PORTS 255
#define XHCI_DCI_MAX 31

/* -- Driver state ----------------------------------------- */

struct xhci_ring {
  struct xhci_trb *trbs;
  uint64_t phys;
  uint32_t enq;
  uint32_t cycle;
};

/** One endpoint's transfer ring; interrupt endpoints also own a buffer */
struct xhci_ep {
  struct xhci_ring ring;
  uint8_t *buf;
  uint64_t buf_phys;
  uint16_t len;
  usb_complete_t complete;
  void *ctx;
};

struct xhci_hc;

struct xhci_dev {
  struct usb_device udev;
  struct xhci_hc *hc;
  uint8_t slot;
  uint8_t *in_ctx;
  uint64_t in_ctx_phys;
  uint8_t *out_ctx;
  uint64_t out_ctx_phys;
  uint8_t *ctrl_buf; /* bounce buffer for control data stages */
  uint64_t ctrl_buf_phys;
  struct xhci_ep *eps[XHCI_DCI_MAX + 1]; /* by device context index */
};

struct xhci_hc {
  uint64_t bar;
  volatile uint8_t *cap, *op, *rt, *db;
  uint32_t max_slots;
  uint32_t max_ports;
  uint32_t ctx_size;
  bool ac64;

  uint64_t *dcbaa;
  uint64_t dcbaa_phys;
  struct xhci_ring cmd;

  volatile struct xhci_trb *evt;
  uint64_t evt_phys;
  uint32_t evt_deq;
  uint32_t evt_cycle;

  struct xhci_dev *slots[256];
  struct xhci_dev *ports[XHCI_MAX_PORTS + 1];
  bool port_pending[XHCI_MAX_PORTS + 1];
};

static const struct usb_hcd_ops xhci_ops;

/* -- MMIO ------------------------------------------------- */

static inline uint32_t rd32(volatile uint8_t *base, uint32_t off) {
  return *(volatile uint32_t *)(base + off);
}

static inline void wr32(volatile uint8_t *base, uint32_t off, uint32_t val) {
  *(volatile uint32_t *)(base + off) = val;
}

/* Two dword writes, low half first: not every controller takes qword
 * accesses. */
static inline void wr64(volatile uint8_t *base, uint32_t off, uint64_t val) {
  wr32(base, off, (uint32_t)val);
  wr32(base, off + 4, (uint32_t)(val >> 32));
}

/**
 * Map [phys, phys + size) uncached at its direct-map address. Ranges
 * the bootloader's direct map already covers are left as they are.
 */
static int xhci_map_mmio(uint64_t phys, uint64_t size) {
  uint64_t end = (phys + size + VMM_PAGE_SIZE - 1) & ~(VMM_PAGE_SIZE - 1ULL);

  for (uint64_t page = phys & ~(VMM_PAGE_SIZE - 1ULL); page < end;
       page += VMM_PAGE_SIZE) {
    uint64_t virt = phys_to_virt(page);
    if (vmm_is_mapped(virt))
      continue;
    if (vmm_map_page(virt, page, VMM_MAP_NO_CACHE) < 0)
      return -1;
  }
  return 0;
}

static bool xhci_wait_reg(volatile uint8_t *base, uint32_t off, uint32_t mask,
                          uint32_t want, uint64_t timeout_ms) {
  for (uint64_t i = 0; i < timeout_ms; i++) {
    if ((rd32(base, off) & mask) == want)
      return true;
    hpet_delay_ms(1);
  }
  return (rd32(base, off) & mask) == want;
}

/* -- DMA memory ------------------------------------------- */

static void *xhci_alloc_page(struct xhci_hc *hc, uint64_t *phys) {
  uint64_t page = pmm_alloc_page();
  if (!page)
    return NULL;
  if (!hc->ac64 && page >= (1ULL << 32)) {
    printk(KERN_ERR "[xhci] page above 4 GiB on a 32-bit controller\n");
    pmm_free_page(page);
    return NULL;
  }

  void *virt = (void *)phys_to_virt(page);
  memset(virt, 0, VMM_PAGE_SIZE);
  *phys = page;
  return virt;
}

/* -- Rings ------------------------------------------------ */

static int xhci_ring_init(struct xhci_hc *hc, struct xhci_ring *ring) {
  ring->trbs = xhci_alloc_page(hc, &ring->phys);
  if (!ring->trbs)
    return -1;

  ring->enq = 0;
  ring->cycle = 1;

  /* The last TRB links back to the start and flips the cycle state. */
  struct xhci_trb *link = &ring->trbs[XHCI_RING_TRBS - 1];
  link->param = ring->phys;
  link->control = TRB_TYPE(TRB_LINK) | TRB_TC;
  return 0;
}

/** Enqueue one TRB; returns its physical address. */
static uint64_t xhci_ring_push(struct xhci_ring *ring, uint64_t param,
                               uint32_t status, uint32_t control) {
  struct xhci_trb *trb = &ring->trbs[ring->enq];
  uint64_t phys = ring->phys + ring->enq * sizeof(struct xhci_trb);

  trb->param = param;
  trb->status = status;
  barrier(); /* the cycle bit hands the TRB over: write it last */
  trb->control = control | ring->cycle;

  if (++ring->enq == XHCI_RING_TRBS - 1) {
    struct xhci_trb *link = &ring->trbs[ring->enq];
    link->control = TRB_TYPE(TRB_LINK) | TRB_TC | ring->cycle;
    ring->enq = 0;
    ring->cycle ^= 1;
  }
  return phys;
}

static void xhci_ring_doorbell(struct xhci_hc *hc, uint8_t slot,
                               uint8_t target) {
  wr32(hc->db, slot * 4, target);
}

/* -- Event ring ------------------------------------------- */

static bool xhci_next_event(struct xhci_hc *hc, struct xhci_trb *out) {
  volatile struct xhci_trb *ev = &hc->evt[hc->evt_deq];
  uint32_t control = ev->control;

  if ((control & TRB_CYCLE) != hc->evt_cycle)
    return false;

  barrier(); /* read the body only after seeing the cycle bit */
  out->param = ev->param;
  out->status = ev->status;
  out->control = control;

  if (++hc->evt_deq == XHCI_RING_TRBS) {
    hc->evt_deq = 0;
    hc->evt_cycle ^= 1;
  }
  wr64(hc->rt, XHCI_ERDP,
       (hc->evt_phys + hc->evt_deq * sizeof(struct xhci_trb)) | ERDP_EHB);
  return true;
}

static void xhci_queue_interrupt(struct xhci_dev *dev, uint8_t dci) {
  struct xhci_ep *ep = dev->eps[dci];
  xhci_ring_push(&ep->ring, ep->buf_phys, ep->len,
                 TRB_TYPE(TRB_NORMAL) | TRB_IOC | TRB_ISP);
  xhci_ring_doorbell(dev->hc, dev->slot, dci);
}

static void xhci_interrupt_done(struct xhci_dev *dev, uint8_t dci,
                                const struct xhci_trb *ev) {
  struct xhci_ep *ep = dev->eps[dci];
  uint32_t cc = TRB_COMPLETION_CODE(ev->status);

  if (cc != CC_SUCCESS && cc != CC_SHORT_PACKET) {
    printk(KERN_ERR "[xhci] slot %d ep %d: interrupt transfer failed, cc=%d\n",
           dev->slot, dci, cc);
    return; /* the endpoint halted; stop polling it */
  }

  uint32_t residue = TRB_TRANSFER_LEN(ev->status);
  int len = residue < ep->len ? (int)(ep->len - residue) : 0;
  ep->complete(&dev->udev, ep->ctx, ep->buf, len);
  xhci_queue_interrupt(dev, dci);
}

/** Handle an event nobody is synchronously waiting for. */
static void xhci_dispatch(struct xhci_hc *hc, const struct xhci_trb *ev) {
  switch (TRB_GET_TYPE(ev->control)) {
  case TRB_EV_PORT_STATUS: {
    uint32_t port = TRB_PORT_ID(ev->param);
    if (port >= 1 && port <= hc->max_ports)
      hc->port_pending[port] = true;
    break;
  }
  case TRB_EV_TRANSFER: {
    struct xhci_dev *dev = hc->slots[TRB_GET_SLOT(ev->control)];
    uint8_t dci = TRB_GET_EP(ev->control);
    if (dev && dev->eps[dci] && dev->eps[dci]->complete)
      xhci_interrupt_done(dev, dci, ev);
    break;
  }
  default:
    break;
  }
}

/* -- Commands and synchronous transfers ------------------- */

/**
 * Wait for the event a caller is blocked on. @p match selects it; every
 * other event is dispatched normally. Returns the completion code, or -1
 * on timeout.
 */
static int xhci_wait_for(struct xhci_hc *hc, uint32_t type, uint64_t cmd_trb,
                         uint8_t slot, uint8_t dci, struct xhci_trb *out) {
  uint64_t deadline = hpet_get_time_ns() + XHCI_TIMEOUT_NS;

  while (hpet_get_time_ns() < deadline) {
    struct xhci_trb ev;
    if (!xhci_next_event(hc, &ev)) {
      cpu_relax();
      continue;
    }

    bool match = TRB_GET_TYPE(ev.control) == type &&
                 (type == TRB_EV_COMMAND ? ev.param == cmd_trb
                                         : TRB_GET_SLOT(ev.control) == slot &&
                                               TRB_GET_EP(ev.control) == dci);
    if (!match) {
      xhci_dispatch(hc, &ev);
      continue;
    }

    if (out)
      *out = ev;
    return (int)TRB_COMPLETION_CODE(ev.status);
  }

  return -1;
}

static int xhci_command(struct xhci_hc *hc, uint64_t param, uint32_t control,
                        struct xhci_trb *out) {
  uint64_t trb = xhci_ring_push(&hc->cmd, param, 0, control);
  xhci_ring_doorbell(hc, 0, 0);

  int cc = xhci_wait_for(hc, TRB_EV_COMMAND, trb, 0, 0, out);
  if (cc != CC_SUCCESS)
    printk(KERN_ERR "[xhci] command %d failed, cc=%d\n", TRB_GET_TYPE(control),
           cc);
  return cc;
}

/** Bring a halted endpoint back and skip the rest of the failed TD. */
static void xhci_reset_endpoint(struct xhci_dev *dev, uint8_t dci) {
  struct xhci_hc *hc = dev->hc;
  struct xhci_ring *ring = &dev->eps[dci]->ring;

  xhci_command(
      hc, 0, TRB_TYPE(TRB_RESET_EP) | TRB_SLOT(dev->slot) | TRB_EP(dci), NULL);

  uint64_t deq = ring->phys + ring->enq * sizeof(struct xhci_trb);
  xhci_command(hc, deq | ring->cycle,
               TRB_TYPE(TRB_SET_TR_DEQUEUE) | TRB_SLOT(dev->slot) | TRB_EP(dci),
               NULL);
}

static int xhci_control(struct usb_device *udev,
                        const struct usb_setup_packet *setup, void *data) {
  struct xhci_dev *dev = udev->hcd_priv;
  struct xhci_hc *hc = dev->hc;
  struct xhci_ring *ring = &dev->eps[DCI_EP0]->ring;
  uint16_t len = setup->wLength;
  bool in = setup->bmRequestType & USB_DIR_IN;

  if (len > VMM_PAGE_SIZE)
    return -1;

  if (len && !in)
    memcpy(dev->ctrl_buf, data, len);
  else if (len)
    memset(dev->ctrl_buf, 0, len);

  uint64_t setup_imm;
  memcpy(&setup_imm, setup, sizeof(setup_imm));

  uint32_t trt = !len ? TRB_TRT_NO_DATA : in ? TRB_TRT_IN : TRB_TRT_OUT;
  xhci_ring_push(ring, setup_imm, sizeof(*setup),
                 TRB_TYPE(TRB_SETUP) | TRB_IDT | trt);
  if (len)
    xhci_ring_push(ring, dev->ctrl_buf_phys, len,
                   TRB_TYPE(TRB_DATA) | (in ? TRB_DIR_IN : 0));
  /* The status stage runs opposite to the data stage (IN if none). */
  xhci_ring_push(ring, 0, 0,
                 TRB_TYPE(TRB_STATUS) | TRB_IOC | (len && in ? 0 : TRB_DIR_IN));
  xhci_ring_doorbell(hc, dev->slot, DCI_EP0);

  int cc = xhci_wait_for(hc, TRB_EV_TRANSFER, 0, dev->slot, DCI_EP0, NULL);
  if (cc != CC_SUCCESS) {
    printk(KERN_ERR "[xhci] slot %d: control request %02x:%02x failed, cc=%d\n",
           dev->slot, setup->bmRequestType, setup->bRequest, cc);
    if (cc > 0)
      xhci_reset_endpoint(dev, DCI_EP0);
    return -1;
  }

  if (len && in)
    memcpy(data, dev->ctrl_buf, len);
  return 0;
}

/* -- Device contexts -------------------------------------- */

/** Input context entry: 0 = input control, 1 = slot, 1 + dci = endpoint */
static uint32_t *xhci_in_ctx(struct xhci_dev *dev, int idx) {
  return (uint32_t *)(dev->in_ctx + idx * dev->hc->ctx_size);
}

/** Output (device) context entry: 0 = slot, dci = endpoint */
static uint32_t *xhci_out_ctx(struct xhci_dev *dev, int idx) {
  return (uint32_t *)(dev->out_ctx + idx * dev->hc->ctx_size);
}

static void xhci_in_ctx_reset(struct xhci_dev *dev, uint32_t add_flags) {
  memset(dev->in_ctx, 0, VMM_PAGE_SIZE);
  xhci_in_ctx(dev, 0)[ICC_ADD] = add_flags;
}

static void xhci_set_ep_dequeue(uint32_t *ep_ctx,
                                const struct xhci_ring *ring) {
  uint64_t deq =
      (ring->phys + ring->enq * sizeof(struct xhci_trb)) | ring->cycle;
  ep_ctx[2] = (uint32_t)deq;
  ep_ctx[3] = (uint32_t)(deq >> 32);
}

static void xhci_fill_ep0(struct xhci_dev *dev, uint16_t max_packet) {
  uint32_t *ep0 = xhci_in_ctx(dev, 1 + DCI_EP0);
  ep0[1] = EP_CERR(3) | EP_TYPE(EP_TYPE_CONTROL) | EP_MAX_PACKET(max_packet);
  xhci_set_ep_dequeue(ep0, &dev->eps[DCI_EP0]->ring);
  ep0[4] = EP_AVG_TRB_LEN(8);
}

static struct xhci_ep *xhci_ep_alloc(struct xhci_hc *hc) {
  struct xhci_ep *ep = kzalloc(sizeof(*ep));
  if (!ep)
    return NULL;
  if (xhci_ring_init(hc, &ep->ring) < 0) {
    kfree(ep);
    return NULL;
  }
  return ep;
}

static void xhci_dev_free(struct xhci_dev *dev) {
  for (int dci = 0; dci <= XHCI_DCI_MAX; dci++) {
    struct xhci_ep *ep = dev->eps[dci];
    if (!ep)
      continue;
    pmm_free_page(ep->ring.phys);
    if (ep->buf)
      pmm_free_page(ep->buf_phys);
    kfree(ep);
  }
  if (dev->in_ctx)
    pmm_free_page(dev->in_ctx_phys);
  if (dev->out_ctx)
    pmm_free_page(dev->out_ctx_phys);
  if (dev->ctrl_buf)
    pmm_free_page(dev->ctrl_buf_phys);
  kfree(dev);
}

/* -- Interrupt endpoints ---------------------------------- */

/** xHCI interval: log2 of the polling period in 125 us frames */
static uint8_t xhci_ep_interval(uint8_t speed, uint8_t b_interval) {
  if (speed == USB_SPEED_HIGH || speed >= USB_SPEED_SUPER) {
    if (b_interval < 1)
      b_interval = 1;
    return b_interval > 16 ? 15 : b_interval - 1;
  }

  /* Full/low speed: bInterval is in 1 ms frames. */
  uint32_t frames = (b_interval ? b_interval : 1) * 8;
  uint8_t exp = 0;
  while ((2u << exp) <= frames)
    exp++;
  return exp < 3 ? 3 : exp > 10 ? 10 : exp;
}

static int xhci_interrupt_in(struct usb_device *udev,
                             const struct usb_endpoint_descriptor *desc,
                             usb_complete_t complete, void *ctx) {
  struct xhci_dev *dev = udev->hcd_priv;
  struct xhci_hc *hc = dev->hc;
  uint8_t num = desc->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
  uint8_t dci = num * 2 + 1;
  uint16_t max_packet = desc->wMaxPacketSize & 0x7FF;
  uint8_t burst =
      udev->speed == USB_SPEED_HIGH ? (desc->wMaxPacketSize >> 11) & 0x3 : 0;

  if (!num || dev->eps[dci] || !max_packet)
    return -1;

  struct xhci_ep *ep = xhci_ep_alloc(hc);
  if (!ep)
    return -1;
  ep->buf = xhci_alloc_page(hc, &ep->buf_phys);
  if (!ep->buf) {
    pmm_free_page(ep->ring.phys);
    kfree(ep);
    return -1;
  }
  ep->len = max_packet;
  dev->eps[dci] = ep;

  xhci_in_ctx_reset(dev, (1u << 0) | (1u << dci));

  /* Slot context: current state, with room for the new endpoint. */
  uint32_t *slot = xhci_in_ctx(dev, 1);
  memcpy(slot, xhci_out_ctx(dev, 0), hc->ctx_size);
  if (SLOT_GET_ENTRIES(slot[0]) < dci)
    slot[0] = (slot[0] & ~SLOT_ENTRIES_MASK) | SLOT_ENTRIES(dci);
  slot[3] = 0; /* device address and slot state are output-only */

  uint32_t esit = max_packet * (burst + 1);
  uint32_t *ep_ctx = xhci_in_ctx(dev, 1 + dci);
  ep_ctx[0] = EP_INTERVAL(xhci_ep_interval(udev->speed, desc->bInterval));
  ep_ctx[1] = EP_CERR(3) | EP_TYPE(EP_TYPE_INT_IN) | EP_MAX_BURST(burst) |
              EP_MAX_PACKET(max_packet);
  xhci_set_ep_dequeue(ep_ctx, &ep->ring);
  ep_ctx[4] = EP_AVG_TRB_LEN(max_packet) | EP_MAX_ESIT_LO(esit);

  if (xhci_command(hc, dev->in_ctx_phys,
                   TRB_TYPE(TRB_CONFIGURE_EP) | TRB_SLOT(dev->slot),
                   NULL) != CC_SUCCESS) {
    dev->eps[dci] = NULL;
    pmm_free_page(ep->buf_phys);
    pmm_free_page(ep->ring.phys);
    kfree(ep);
    return -1;
  }

  ep->complete = complete;
  ep->ctx = ctx;
  xhci_queue_interrupt(dev, dci);
  return 0;
}

static const struct usb_hcd_ops xhci_ops = {
    .control = xhci_control,
    .interrupt_in = xhci_interrupt_in,
};

/* -- Device attach / detach ------------------------------- */

static uint16_t xhci_default_max_packet(uint8_t speed) {
  switch (speed) {
  case USB_SPEED_HIGH:
    return 64;
  case USB_SPEED_LOW:
  case USB_SPEED_FULL:
    return 8;
  default:
    return 512;
  }
}

/**
 * Full/low-speed devices may have a larger EP0 than the default 8 bytes:
 * read it from the first 8 bytes of the device descriptor and tell the
 * controller.
 */
static int xhci_fix_ep0_size(struct xhci_dev *dev) {
  struct usb_device_descriptor head;

  if (usb_control_msg(
          &dev->udev, USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
          USB_REQ_GET_DESCRIPTOR, USB_DESC_DEVICE << 8, 0, &head, 8) < 0)
    return -1;

  uint16_t size = head.bMaxPacketSize0;
  if (size == 8 || (size != 16 && size != 32 && size != 64))
    return 0;

  xhci_in_ctx_reset(dev, 1u << DCI_EP0);
  xhci_fill_ep0(dev, size);
  return xhci_command(dev->hc, dev->in_ctx_phys,
                      TRB_TYPE(TRB_EVALUATE_CONTEXT) | TRB_SLOT(dev->slot),
                      NULL) == CC_SUCCESS
             ? 0
             : -1;
}

static void xhci_attach(struct xhci_hc *hc, uint8_t port, uint32_t portsc) {
  uint8_t speed = PORTSC_SPEED(portsc);
  struct xhci_trb ev;

  if (xhci_command(hc, 0, TRB_TYPE(TRB_ENABLE_SLOT), &ev) != CC_SUCCESS)
    return;
  uint8_t slot_id = TRB_GET_SLOT(ev.control);

  struct xhci_dev *dev = kzalloc(sizeof(*dev));
  if (!dev)
    goto fail;
  dev->hc = hc;
  dev->slot = slot_id;
  dev->udev.ops = &xhci_ops;
  dev->udev.hcd_priv = dev;
  dev->udev.speed = speed;
  dev->udev.port = port;

  dev->in_ctx = xhci_alloc_page(hc, &dev->in_ctx_phys);
  dev->out_ctx = xhci_alloc_page(hc, &dev->out_ctx_phys);
  dev->ctrl_buf = xhci_alloc_page(hc, &dev->ctrl_buf_phys);
  dev->eps[DCI_EP0] = xhci_ep_alloc(hc);
  if (!dev->in_ctx || !dev->out_ctx || !dev->ctrl_buf || !dev->eps[DCI_EP0])
    goto fail;

  hc->dcbaa[slot_id] = dev->out_ctx_phys;
  hc->slots[slot_id] = dev;

  xhci_in_ctx_reset(dev, (1u << 0) | (1u << DCI_EP0));
  uint32_t *slot = xhci_in_ctx(dev, 1);
  slot[0] = SLOT_SPEED(speed) | SLOT_ENTRIES(1);
  slot[1] = SLOT_ROOT_PORT(port);
  xhci_fill_ep0(dev, xhci_default_max_packet(speed));

  /* Address Device also sends SET_ADDRESS to the device. */
  if (xhci_command(hc, dev->in_ctx_phys,
                   TRB_TYPE(TRB_ADDRESS_DEVICE) | TRB_SLOT(slot_id),
                   NULL) != CC_SUCCESS)
    goto fail;
  hpet_delay_ms(2); /* SET_ADDRESS recovery interval */

  if (speed < USB_SPEED_HIGH && xhci_fix_ep0_size(dev) < 0)
    goto fail;

  printk(KERN_INFO "[xhci] port %d: device in slot %d, speed %d\n", port,
         slot_id, speed);

  hc->ports[port] = dev;
  usb_probe_device(&dev->udev);
  return;

fail:
  /* Disable the slot first: until then the controller may still use the
   * device's context and rings. */
  xhci_command(hc, 0, TRB_TYPE(TRB_DISABLE_SLOT) | TRB_SLOT(slot_id), NULL);
  hc->slots[slot_id] = NULL;
  hc->dcbaa[slot_id] = 0;
  if (dev)
    xhci_dev_free(dev);
}

static void xhci_detach(struct xhci_hc *hc, uint8_t port) {
  struct xhci_dev *dev = hc->ports[port];
  hc->ports[port] = NULL;

  printk(KERN_INFO "[xhci] port %d: device disconnected\n", port);

  /* No more reports: events still queued for this slot are dispatched
   * while Disable Slot runs, after the class drivers are gone. */
  for (int dci = 0; dci <= XHCI_DCI_MAX; dci++)
    if (dev->eps[dci])
      dev->eps[dci]->complete = NULL;
  usb_disconnect_device(&dev->udev);

  /* Once Disable Slot completes the controller no longer touches the
   * device's rings and contexts. */
  xhci_command(hc, 0, TRB_TYPE(TRB_DISABLE_SLOT) | TRB_SLOT(dev->slot), NULL);
  hc->slots[dev->slot] = NULL;
  hc->dcbaa[dev->slot] = 0;
  xhci_dev_free(dev);
}

/* -- Root hub ports --------------------------------------- */

static int xhci_port_reset(struct xhci_hc *hc, uint8_t port) {
  uint32_t sc = rd32(hc->op, XHCI_PORTSC(port));
  wr32(hc->op, XHCI_PORTSC(port), (sc & PORTSC_PRESERVE) | PORTSC_PR);

  if (!xhci_wait_reg(hc->op, XHCI_PORTSC(port), PORTSC_PRC, PORTSC_PRC, 500)) {
    printk(KERN_ERR "[xhci] port %d: reset timed out\n", port);
    return -1;
  }

  sc = rd32(hc->op, XHCI_PORTSC(port));
  wr32(hc->op, XHCI_PORTSC(port), (sc & PORTSC_PRESERVE) | PORTSC_PRC);
  hpet_delay_ms(10); /* reset recovery */

  return (rd32(hc->op, XHCI_PORTSC(port)) & PORTSC_PED) ? 0 : -1;
}

static void xhci_port_check(struct xhci_hc *hc, uint8_t port) {
  uint32_t sc = rd32(hc->op, XHCI_PORTSC(port));
  wr32(hc->op, XHCI_PORTSC(port),
       (sc & PORTSC_PRESERVE) | (sc & PORTSC_CHANGES));

  /* Unplugging disables the port, so a disabled port means the attached
   * device is gone even if another one is already plugged in again. */
  if (hc->ports[port] &&
      (sc & (PORTSC_CCS | PORTSC_PED)) != (PORTSC_CCS | PORTSC_PED))
    xhci_detach(hc, port);

  if (!(sc & PORTSC_CCS) || hc->ports[port])
    return;

  /* USB 3 ports enable themselves; USB 2 ports need a reset. */
  if (!(sc & PORTSC_PED) && xhci_port_reset(hc, port) < 0) {
    printk(KERN_ERR "[xhci] port %d: failed to enable\n", port);
    return;
  }

  xhci_attach(hc, port, rd32(hc->op, XHCI_PORTSC(port)));
}

/* -- Polling thread --------------------------------------- */

static void xhci_thread(void *arg) {
  struct xhci_hc *hc = arg;

  for (;;) {
    struct xhci_trb ev;
    while (xhci_next_event(hc, &ev))
      xhci_dispatch(hc, &ev);

    for (uint32_t port = 1; port <= hc->max_ports; port++) {
      if (!hc->port_pending[port])
        continue;
      hc->port_pending[port] = false;
      xhci_port_check(hc, port);
    }

    task_prepare_wait();
    task_wait(hpet_get_time_ns() + XHCI_POLL_NS);
  }
}

/* -- Controller bring-up ---------------------------------- */

/** Take the controller from the BIOS and stop its legacy-emulation SMIs. */
static void xhci_bios_handoff(struct xhci_hc *hc) {
  uint32_t off = HCC1_XECP(rd32(hc->cap, XHCI_HCCPARAMS1)) * 4;

  while (off) {
    /* Extended capabilities can sit far past the registers mapped at
     * probe (0x8000+ on real controllers): map each one before use. */
    if (xhci_map_mmio(hc->bar + off, 8) < 0)
      return;
    uint32_t cap = rd32(hc->cap, off);

    if (XECP_ID(cap) == XECP_ID_LEGACY) {
      if (cap & USBLEGSUP_BIOS_OWNED) {
        wr32(hc->cap, off, cap | USBLEGSUP_OS_OWNED);
        if (!xhci_wait_reg(hc->cap, off, USBLEGSUP_BIOS_OWNED, 0, 1000))
          printk(KERN_WARNING "[xhci] BIOS did not release the controller\n");
      }
      uint32_t ctl = rd32(hc->cap, off + 4);
      wr32(hc->cap, off + 4,
           (ctl & ~USBLEGCTLSTS_SMI_ENABLES) | USBLEGCTLSTS_SMI_EVENTS);
      return;
    }

    off = XECP_NEXT(cap) ? off + XECP_NEXT(cap) * 4 : 0;
  }
}

static int xhci_reset(struct xhci_hc *hc) {
  wr32(hc->op, XHCI_USBCMD, rd32(hc->op, XHCI_USBCMD) & ~USBCMD_RUN);
  if (!xhci_wait_reg(hc->op, XHCI_USBSTS, USBSTS_HCH, USBSTS_HCH, 100))
    return -1;

  wr32(hc->op, XHCI_USBCMD, USBCMD_HCRST);
  if (!xhci_wait_reg(hc->op, XHCI_USBCMD, USBCMD_HCRST, 0, 1000))
    return -1;
  return xhci_wait_reg(hc->op, XHCI_USBSTS, USBSTS_CNR, 0, 1000) ? 0 : -1;
}

static int xhci_setup_scratchpad(struct xhci_hc *hc) {
  uint32_t count = HCS2_MAX_SCRATCHPAD(rd32(hc->cap, XHCI_HCSPARAMS2));
  if (!count)
    return 0;
  if (count > VMM_PAGE_SIZE / sizeof(uint64_t))
    return -1;

  uint64_t array_phys;
  uint64_t *array = xhci_alloc_page(hc, &array_phys);
  if (!array)
    return -1;

  for (uint32_t i = 0; i < count; i++) {
    uint64_t page;
    if (!xhci_alloc_page(hc, &page))
      return -1;
    array[i] = page;
  }

  hc->dcbaa[0] = array_phys;
  return 0;
}

static int xhci_setup_rings(struct xhci_hc *hc) {
  hc->dcbaa = xhci_alloc_page(hc, &hc->dcbaa_phys);
  if (!hc->dcbaa || xhci_setup_scratchpad(hc) < 0)
    return -1;

  if (xhci_ring_init(hc, &hc->cmd) < 0)
    return -1;

  uint64_t evt_phys, erst_phys;
  hc->evt = xhci_alloc_page(hc, &evt_phys);
  uint64_t *erst = xhci_alloc_page(hc, &erst_phys);
  if (!hc->evt || !erst)
    return -1;
  hc->evt_phys = evt_phys;
  hc->evt_deq = 0;
  hc->evt_cycle = 1;

  /* One event ring segment: base address, then size in TRBs. */
  erst[0] = evt_phys;
  erst[1] = XHCI_RING_TRBS;

  wr32(hc->op, XHCI_CONFIG, hc->max_slots);
  wr64(hc->op, XHCI_DCBAAP, hc->dcbaa_phys);
  wr64(hc->op, XHCI_CRCR, hc->cmd.phys | CRCR_RCS);

  wr32(hc->rt, XHCI_ERSTSZ, 1);
  wr64(hc->rt, XHCI_ERDP, evt_phys);
  wr64(hc->rt, XHCI_ERSTBA, erst_phys);
  return 0;
}

static void xhci_power_ports(struct xhci_hc *hc) {
  if (!(rd32(hc->cap, XHCI_HCCPARAMS1) & HCC1_PPC))
    return;

  for (uint32_t port = 1; port <= hc->max_ports; port++) {
    uint32_t sc = rd32(hc->op, XHCI_PORTSC(port));
    if (!(sc & PORTSC_PP))
      wr32(hc->op, XHCI_PORTSC(port), (sc & PORTSC_PRESERVE) | PORTSC_PP);
  }
  hpet_delay_ms(20); /* power-on to power-good */
}

static int xhci_probe(struct pci_device *pci) {
  uint64_t bar = pci->bar0;
  if (!bar) {
    printk(KERN_ERR "[xhci] no MMIO BAR\n");
    return -1;
  }

  pci_set_command(pci, PCI_COMMAND_MEM | PCI_COMMAND_MASTER |
                           PCI_COMMAND_INT_DISABLE);

  if (xhci_map_mmio(bar, VMM_PAGE_SIZE) < 0)
    return -1;

  struct xhci_hc *hc = kzalloc(sizeof(*hc));
  if (!hc)
    return -1;

  hc->bar = bar;
  hc->cap = (volatile uint8_t *)phys_to_virt(bar);
  uint32_t caplen = rd32(hc->cap, XHCI_CAPLENGTH) & 0xFF;
  uint32_t hcs1 = rd32(hc->cap, XHCI_HCSPARAMS1);
  uint32_t hcc1 = rd32(hc->cap, XHCI_HCCPARAMS1);
  uint32_t dboff = rd32(hc->cap, XHCI_DBOFF) & ~0x3u;
  uint32_t rtsoff = rd32(hc->cap, XHCI_RTSOFF) & ~0x1Fu;

  hc->max_slots = HCS1_MAX_SLOTS(hcs1);
  hc->max_ports = HCS1_MAX_PORTS(hcs1);
  hc->ctx_size = (hcc1 & HCC1_CSZ) ? 64 : 32;
  hc->ac64 = hcc1 & HCC1_AC64;

  /* Everything the driver touches: ports, interrupter 0, doorbells. */
  uint64_t span = caplen + XHCI_PORTSC(hc->max_ports + 1);
  if (rtsoff + XHCI_IR0 + 0x20 > span)
    span = rtsoff + XHCI_IR0 + 0x20;
  if (dboff + 4 * (hc->max_slots + 1) > span)
    span = dboff + 4 * (hc->max_slots + 1);
  if (xhci_map_mmio(bar, span) < 0) {
    kfree(hc);
    return -1;
  }

  hc->op = hc->cap + caplen;
  hc->rt = hc->cap + rtsoff;
  hc->db = hc->cap + dboff;

  printk(KERN_INFO "[xhci] version %x, %d slots, %d ports, %d-byte contexts\n",
         rd32(hc->cap, XHCI_CAPLENGTH) >> 16, hc->max_slots, hc->max_ports,
         hc->ctx_size);

  xhci_bios_handoff(hc);

  if (xhci_reset(hc) < 0) {
    printk(KERN_ERR "[xhci] controller reset failed\n");
    kfree(hc);
    return -1;
  }

  /* On failure past this point the controller stays halted and its
   * pages are not reclaimed. */
  if (xhci_setup_rings(hc) < 0) {
    printk(KERN_ERR "[xhci] out of memory\n");
    return -1;
  }

  wr32(hc->op, XHCI_USBCMD, USBCMD_RUN);
  if (!xhci_wait_reg(hc->op, XHCI_USBSTS, USBSTS_HCH, 0, 100)) {
    printk(KERN_ERR "[xhci] controller did not start\n");
    return -1;
  }

  xhci_power_ports(hc);
  for (uint32_t port = 1; port <= hc->max_ports; port++)
    hc->port_pending[port] = true;

  task_t *thread =
      _task_create_with_arg(xhci_thread, hc, XHCI_THREAD_PRIORITY, false);
  if (!thread) {
    printk(KERN_ERR "[xhci] failed to start the polling thread\n");
    return -1;
  }
  scheduler_add_task(thread);

  printk(KERN_OK "[xhci] controller running\n");
  return 0;
}

/* -- Driver registration ---------------------------------- */

#define PCI_CLASS_XHCI 0x0C0330 /* serial bus / USB / xHCI */

static const struct pci_device_id xhci_ids[] = {
    PCI_DEVICE_CLASS(PCI_CLASS_XHCI, 0xFFFFFF),
    {0},
};

static struct pci_driver xhci_driver = {
    .name = "xhci",
    .id_table = xhci_ids,
    .probe = xhci_probe,
};

static int xhci_module_init(void) { return pci_register_driver(&xhci_driver); }

module_init(xhci_module_init);
MODULE_NAME("usb");
MODULE_DESCRIPTION("USB core, xHCI host controller and HID boot keyboard");
