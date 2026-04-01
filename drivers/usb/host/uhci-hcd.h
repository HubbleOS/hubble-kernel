#pragma once

/**
 * @file uhci-hcd.h
 * @brief Universal Host Controller Interface for Host Controller Driver
 */

#define USB_COMMAND 0x00
#define USB_STATUS 0x02
#define USBINTR 0x04
#define USBFRNUM 0x06
#define USBADDR 0x08
#define SOFMOD 0x0C
#define USBPORTSC1 0x10
#define USBPORTSC2 0x12
