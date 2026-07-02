/**
 * @file uhci-hcd.h
 * @brief UHCI host controller register offsets and bit definitions
 */
#pragma once

/* -- I/O register offsets (from I/O base) ----------------- */

#define USBCMD 0x00
#define USBSTS 0x02
#define USBINTR 0x04
#define FRNUM 0x06
#define FRBASEADD 0x08
#define SOFMOD 0x0C
#define PORTSC1 0x10
#define PORTSC2 0x12

/* -- Command register (USBCMD) bits ----------------------- */

#define GRESET (1 << 2)
#define HCRESET (1 << 1)
#define RS (1 << 0)
#define USBCMD_RS (1 << 0)

/* -- Port status / control bits --------------------------- */

#define PORTSC_CSC (1 << 1)
#define PORTSC_CS (1 << 0)
#define PORTSC_PE (1 << 2)
#define PORTSC_RESET (1 << 9)
