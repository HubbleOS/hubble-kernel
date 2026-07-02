/**
 * @file uhci-td.h
 * @brief UHCI transfer descriptor (TD), queue head (QH), and setup-packet types
 */
#pragma once

#include <stdint.h>

/* -- Transfer Descriptor (TD) — 16-byte aligned ----------- */

/** @brief UHCI Transfer Descriptor (must be 16-byte aligned) */
struct uhci_td {
  uint32_t link;
  uint32_t status;
  uint32_t token;
  uint32_t buffer;
} __attribute__((packed, aligned(16)));

/* link field */
#define TD_LINK_TERMINATE (1 << 0)
#define TD_LINK_QH (1 << 1)
#define TD_LINK_DEPTH (1 << 2)

/* status field */
#define TD_STATUS_ERRCNT(n) ((n) << 27)
#define TD_STATUS_LS (1 << 26)
#define TD_STATUS_ISO (1 << 25)
#define TD_STATUS_IOC (1 << 24)
#define TD_STATUS_ACTIVE (1 << 23)
#define TD_STATUS_STALLED (1 << 22)
#define TD_STATUS_DBUFERR (1 << 21)
#define TD_STATUS_BABBLE (1 << 20)
#define TD_STATUS_NAK (1 << 19)
#define TD_STATUS_CRCTO (1 << 18)
#define TD_STATUS_BITSTUFF (1 << 17)

/* token: PID */
#define TD_PID_SETUP 0x2D
#define TD_PID_IN 0x69
#define TD_PID_OUT 0xE1

/* token assembly */
#define TD_TOKEN(pid, addr, ep, toggle, maxlen)                                \
  (((uint32_t)(maxlen) << 21) | ((uint32_t)(toggle) << 19) |                   \
   ((uint32_t)(ep) << 15) | ((uint32_t)(addr) << 8) | ((uint32_t)(pid)))

/* -- Queue Head (QH) — 16-byte aligned -------------------- */

/** @brief UHCI Queue Head (must be 16-byte aligned) */
struct uhci_qh {
  uint32_t head_link;
  uint32_t element_link;
} __attribute__((packed, aligned(16)));

/* -- USB Setup Packet (8 bytes) --------------------------- */

/** @brief Standard USB setup packet for control transfers */
struct usb_setup_packet {
  uint8_t bmRequestType;
  uint8_t bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
} __attribute__((packed));
