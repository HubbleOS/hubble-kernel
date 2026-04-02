#pragma once

#include <stdint.h>

/* ──────────────────────────────────────────
 * Transfer Descriptor (TD)
 * Вирівнювання: 16 байт (вимога UHCI)
 * ────────────────────────────────────────── */
struct uhci_td
{
	uint32_t link;	 // вказівник на наступний TD/QH
	uint32_t status; // статус + control bits
	uint32_t token;	 // PID + адреса + endpoint + довжина
	uint32_t buffer; // фізична адреса буфера даних
} __attribute__((packed, aligned(16)));

/* link */
#define TD_LINK_TERMINATE (1 << 0) // T=1: немає наступного елемента
#define TD_LINK_QH (1 << 1)	   // наступний елемент — QH (не TD)
#define TD_LINK_DEPTH (1 << 2)	   // depth-first обхід

/* status */
#define TD_STATUS_ERRCNT(n) ((n) << 27) // bits 28:27
#define TD_STATUS_LS (1 << 26)		// Low Speed Device
#define TD_STATUS_ISO (1 << 25)		// Isochronous
#define TD_STATUS_IOC (1 << 24)		// Interrupt on Complete
#define TD_STATUS_ACTIVE (1 << 23)	// Active
#define TD_STATUS_STALLED (1 << 22)	// Stalled
#define TD_STATUS_DBUFERR (1 << 21)	// Data Buffer Error
#define TD_STATUS_BABBLE (1 << 20)	// Babble Detected
#define TD_STATUS_NAK (1 << 19)		// NAK Received
#define TD_STATUS_CRCTO (1 << 18)	// CRC/Timeout Error
#define TD_STATUS_BITSTUFF (1 << 17)	// Bit Stuff Error

/* token: PID */
#define TD_PID_SETUP 0x2D
#define TD_PID_IN 0x69
#define TD_PID_OUT 0xE1

/* token: збірка поля token */
#define TD_TOKEN(pid, addr, ep, toggle, maxlen) \
	(((uint32_t)(maxlen) << 21) |           \
	 ((uint32_t)(toggle) << 19) |           \
	 ((uint32_t)(ep) << 15) |               \
	 ((uint32_t)(addr) << 8) |              \
	 ((uint32_t)(pid)))

/* ──────────────────────────────────────────
 * Queue Head (QH)
 * Вирівнювання: 16 байт (вимога UHCI)
 * ────────────────────────────────────────── */
struct uhci_qh
{
	uint32_t head_link;    // горизонтальний вказівник: наступний QH
	uint32_t element_link; // вертикальний вказівник: перший TD
} __attribute__((packed, aligned(16)));

/* ──────────────────────────────────────────
 * USB Setup Packet (Control Transfer)
 * Завжди 8 байт — стандарт USB
 * ────────────────────────────────────────── */
struct usb_setup_packet
{
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} __attribute__((packed));
