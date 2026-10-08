/**
 * @file xhci.h
 * @brief xHCI (USB 3.x host controller) registers, TRBs and contexts
 *
 * Register and field names follow the xHCI 1.2 specification.
 */
#pragma once

#include <stdint.h>

/* -- Capability registers (BAR0) -------------------------- */

#define XHCI_CAPLENGTH 0x00
#define XHCI_HCSPARAMS1 0x04
#define XHCI_HCSPARAMS2 0x08
#define XHCI_HCCPARAMS1 0x10
#define XHCI_DBOFF 0x14
#define XHCI_RTSOFF 0x18

#define HCS1_MAX_SLOTS(p) ((p) & 0xFF)
#define HCS1_MAX_PORTS(p) (((p) >> 24) & 0xFF)
#define HCS2_MAX_SCRATCHPAD(p)                                                 \
  ((((p) >> 21) & 0x1F) << 5 | (((p) >> 27) & 0x1F))
#define HCC1_AC64 (1 << 0)
#define HCC1_CSZ (1 << 2)
#define HCC1_PPC (1 << 3)
#define HCC1_XECP(p) (((p) >> 16) & 0xFFFF)

/* -- Operational registers (BAR0 + CAPLENGTH) ------------- */

#define XHCI_USBCMD 0x00
#define XHCI_USBSTS 0x04
#define XHCI_CRCR 0x18
#define XHCI_DCBAAP 0x30
#define XHCI_CONFIG 0x38
#define XHCI_PORTSC(n) (0x400 + 0x10 * ((n) - 1))

#define USBCMD_RUN (1 << 0)
#define USBCMD_HCRST (1 << 1)

#define USBSTS_HCH (1 << 0)
#define USBSTS_HSE (1 << 2)
#define USBSTS_CNR (1 << 11)

#define CRCR_RCS (1 << 0)

/* -- PORTSC ----------------------------------------------- */

#define PORTSC_CCS (1 << 0)
#define PORTSC_PED (1 << 1)
#define PORTSC_PR (1 << 4)
#define PORTSC_PP (1 << 9)
#define PORTSC_SPEED(p) (((p) >> 10) & 0xF)
#define PORTSC_CSC (1 << 17)
#define PORTSC_PEC (1 << 18)
#define PORTSC_WRC (1 << 19)
#define PORTSC_OCC (1 << 20)
#define PORTSC_PRC (1 << 21)
#define PORTSC_PLC (1 << 22)
#define PORTSC_CEC (1 << 23)
#define PORTSC_CHANGES                                                         \
  (PORTSC_CSC | PORTSC_PEC | PORTSC_WRC | PORTSC_OCC | PORTSC_PRC |            \
   PORTSC_PLC | PORTSC_CEC)
/* Read-only and read/write-preserve bits: everything that is safe to
 * write back unchanged. PED and the change bits are write-1-to-clear. */
#define PORTSC_PRESERVE 0x4E00FFE9u

/* -- Runtime registers (BAR0 + RTSOFF), interrupter 0 ----- */

#define XHCI_IR0 0x20
#define XHCI_IMAN (XHCI_IR0 + 0x00)
#define XHCI_ERSTSZ (XHCI_IR0 + 0x08)
#define XHCI_ERSTBA (XHCI_IR0 + 0x10)
#define XHCI_ERDP (XHCI_IR0 + 0x18)

#define ERDP_EHB (1 << 3)

/* -- Extended capabilities -------------------------------- */

#define XECP_ID(v) ((v) & 0xFF)
#define XECP_NEXT(v) (((v) >> 8) & 0xFF)
#define XECP_ID_LEGACY 1
#define USBLEGSUP_BIOS_OWNED (1 << 16)
#define USBLEGSUP_OS_OWNED (1 << 24)
/* USBLEGCTLSTS: SMI enables, and the write-1-to-clear SMI events */
#define USBLEGCTLSTS_SMI_ENABLES 0x0000E011u
#define USBLEGCTLSTS_SMI_EVENTS 0xE0000000u

/* -- Transfer Request Blocks ------------------------------ */

struct xhci_trb {
  uint64_t param;
  uint32_t status;
  uint32_t control;
} __attribute__((packed));

#define TRB_CYCLE (1 << 0)
#define TRB_TC (1 << 1)  /* Link: toggle cycle */
#define TRB_ISP (1 << 2) /* Interrupt on short packet */
#define TRB_IOC (1 << 5)
#define TRB_IDT (1 << 6) /* Immediate data (setup packet) */
#define TRB_DIR_IN (1 << 16)
#define TRB_TYPE(t) ((uint32_t)(t) << 10)
#define TRB_GET_TYPE(c) (((c) >> 10) & 0x3F)
#define TRB_SLOT(s) ((uint32_t)(s) << 24)
#define TRB_GET_SLOT(c) (((c) >> 24) & 0xFF)
#define TRB_EP(e) ((uint32_t)(e) << 16)
#define TRB_GET_EP(c) (((c) >> 16) & 0x1F)

/* Setup stage transfer type */
#define TRB_TRT_NO_DATA (0 << 16)
#define TRB_TRT_OUT (2 << 16)
#define TRB_TRT_IN (3 << 16)

#define TRB_COMPLETION_CODE(s) (((s) >> 24) & 0xFF)
#define TRB_TRANSFER_LEN(s) ((s) & 0xFFFFFF)
#define TRB_PORT_ID(p) (((p) >> 24) & 0xFF)

enum {
  TRB_NORMAL = 1,
  TRB_SETUP = 2,
  TRB_DATA = 3,
  TRB_STATUS = 4,
  TRB_LINK = 6,
  TRB_ENABLE_SLOT = 9,
  TRB_DISABLE_SLOT = 10,
  TRB_ADDRESS_DEVICE = 11,
  TRB_CONFIGURE_EP = 12,
  TRB_EVALUATE_CONTEXT = 13,
  TRB_RESET_EP = 14,
  TRB_SET_TR_DEQUEUE = 16,
  TRB_EV_TRANSFER = 32,
  TRB_EV_COMMAND = 33,
  TRB_EV_PORT_STATUS = 34,
};

enum {
  CC_SUCCESS = 1,
  CC_STALL = 6,
  CC_SHORT_PACKET = 13,
};

/* -- Contexts (32-byte layout; CSZ doubles the stride) ---- */

/* Input control context */
#define ICC_DROP 0
#define ICC_ADD 1

/* Slot context dwords */
#define SLOT_SPEED(s) ((uint32_t)(s) << 20)
#define SLOT_ENTRIES(n) ((uint32_t)(n) << 27)
#define SLOT_GET_ENTRIES(d) (((d) >> 27) & 0x1F)
#define SLOT_ENTRIES_MASK (0x1Fu << 27)
#define SLOT_ROOT_PORT(p) ((uint32_t)(p) << 16)

/* Endpoint context dwords */
#define EP_INTERVAL(i) ((uint32_t)(i) << 16)
#define EP_CERR(c) ((uint32_t)(c) << 1)
#define EP_TYPE(t) ((uint32_t)(t) << 3)
#define EP_MAX_BURST(b) ((uint32_t)(b) << 8)
#define EP_MAX_PACKET(p) ((uint32_t)(p) << 16)
#define EP_AVG_TRB_LEN(l) ((uint32_t)(l))
#define EP_MAX_ESIT_LO(p) ((uint32_t)(p) << 16)

#define EP_TYPE_CONTROL 4
#define EP_TYPE_INT_IN 7

/* Device context index of endpoint 0 */
#define DCI_EP0 1
