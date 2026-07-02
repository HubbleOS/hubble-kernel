/**
 * @file e1000.h
 * @brief Intel e1000 network adapter — register definitions and descriptor
 * structs
 */
#pragma once

#include <stdint.h>

/* -- PCI IDs ---------------------------------------------- */

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E

/* -- MMIO register offsets -------------------------------- */

#define E1000_CTRL 0x0000
#define E1000_STATUS 0x0008

#define E1000_CTRL_RST (1 << 26)
#define E1000_CTRL_SLU (1 << 6)
#define E1000_CTRL_ILOS (1 << 7)
#define E1000_CTRL_LOOP (1 << 8)

#define E1000_RAL0 0x5400
#define E1000_RAH0 0x5404

#define E1000_RDBAL 0x2800
#define E1000_RDBAH 0x2804
#define E1000_RDLEN 0x2808
#define E1000_RDH 0x2810
#define E1000_RDT 0x2818
#define E1000_RCTL 0x0100

#define E1000_TDBAL 0x3800
#define E1000_TDBAH 0x3804
#define E1000_TDLEN 0x3808
#define E1000_TDH 0x3810
#define E1000_TDT 0x3818
#define E1000_TCTL 0x0400

/* -- RCTL bits -------------------------------------------- */

#define E1000_RCTL_EN (1 << 1)
#define E1000_RCTL_BAM (1 << 15)
#define E1000_RCTL_BSIZE_2048 0

/* -- TCTL bits -------------------------------------------- */

#define E1000_TCTL_EN (1 << 1)
#define E1000_TCTL_PSP (1 << 3)
#define E1000_TCTL_CT (0x0F << 4)
#define E1000_TCTL_COLD (0x040 << 12)

/* -- Ring parameters -------------------------------------- */

#define E1000_RX_DESC_COUNT 32
#define E1000_TX_DESC_COUNT 32
#define E1000_BUFFER_SIZE 2048
#define E1000_MMIO_SIZE 0x20000

/* -- Descriptor flags ------------------------------------- */

#define E1000_TX_CMD_EOP (1 << 0)
#define E1000_TX_CMD_RS (1 << 3)
#define E1000_TX_STAT_DD (1 << 0)

#define E1000_RX_STAT_DD (1 << 0)
#define E1000_RX_STAT_EOP (1 << 1)

/* -- Descriptor structures -------------------------------- */

/** @brief e1000 RX descriptor (16 bytes, little-endian) */
struct e1000_rx_desc {
  volatile uint64_t addr;
  volatile uint16_t length;
  volatile uint16_t checksum;
  volatile uint8_t status;
  volatile uint8_t errors;
  volatile uint16_t special;
} __attribute__((packed));

/** @brief e1000 TX descriptor (16 bytes, little-endian) */
struct e1000_tx_desc {
  volatile uint64_t addr;
  volatile uint16_t length;
  volatile uint8_t cso;
  volatile uint8_t cmd;
  volatile uint8_t status;
  volatile uint8_t css;
  volatile uint16_t special;
} __attribute__((packed));

/* -- API -------------------------------------------------- */

/**
 * @brief Transmit a packet
 * @param data  Packet data (must remain valid until DD is set)
 * @param len   Packet length in bytes
 * @return 0 on success
 */
int e1000_send(const void *data, uint16_t len);

/**
 * @brief Receive a packet (non-blocking)
 * @param buf     Destination buffer
 * @param len_out  Receives the packet length
 * @return 0 on success, -1 if no packet is available
 */
int e1000_recv(void *buf, uint16_t *len_out);

/**
 * @brief Copy the adapter MAC address into the provided buffer
 * @param mac  Output buffer (must be at least 6 bytes)
 */
void e1000_get_mac(uint8_t mac[6]);
