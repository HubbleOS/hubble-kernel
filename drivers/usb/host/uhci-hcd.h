#pragma once

/**
 * @file uhci-hcd.h
 * @brief Universal Host Controller Interface for Host Controller Driver
 */

#define USBCMD 0x00    // USB Command
#define USBSTS 0x02    // USB Status
#define USBINTR 0x04   // USB Interrupt
#define FRNUM 0x06     // Frame Number
#define FRBASEADD 0x08 // Frame List Base Address
#define SOFMOD 0x0C    // Start Of Frame Modify
#define PORTSC1 0x10   // Port 1 Status/Control
#define PORTSC2 0x12   // Port 2 Status/Control

// Command Register
/* bits 8–15 reserved */
#define GRESET (1 << 2)	 // Global Reset
#define HCRESET (1 << 1) // Host Controller Reset
#define RS (1 << 0)	 // Run/Stop
