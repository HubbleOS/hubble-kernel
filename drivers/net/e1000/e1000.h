/**
 * @file e1000.h
 * @brief Intel 8254x (e1000) and 82574 (e1000e) Ethernet: registers and
 *        descriptors
 *
 * Both families share the registers and the legacy descriptor format
 * used here; they differ in how the EEPROM is read.
 */
#pragma once

#include <stdint.h>

/* -- Registers -------------------------------------------- */

#define E1000_CTRL 0x0000
#define E1000_STATUS 0x0008
#define E1000_EECD 0x0010
#define E1000_EERD 0x0014
#define E1000_ICR 0x00C0
#define E1000_IMS 0x00D0
#define E1000_IMC 0x00D8
#define E1000_RCTL 0x0100
#define E1000_TCTL 0x0400
#define E1000_TIPG 0x0410
#define E1000_RDBAL 0x2800
#define E1000_RDBAH 0x2804
#define E1000_RDLEN 0x2808
#define E1000_RDH 0x2810
#define E1000_RDT 0x2818
#define E1000_RDTR 0x2820 /* receive delay timer */
#define E1000_TDBAL 0x3800
#define E1000_TDBAH 0x3804
#define E1000_TDLEN 0x3808
#define E1000_TDH 0x3810
#define E1000_TDT 0x3818
#define E1000_MTA 0x5200 /* 128 entries */
#define E1000_RAL0 0x5400
#define E1000_RAH0 0x5404

#define E1000_CTRL_ASDE (1u << 5)
#define E1000_CTRL_SLU (1u << 6)
#define E1000_CTRL_ILOS (1u << 7)
#define E1000_CTRL_RST (1u << 26)
#define E1000_CTRL_VME (1u << 30)
#define E1000_CTRL_PHY_RST (1u << 31)

#define E1000_STATUS_LU (1u << 1)

/* ICR / IMS / IMC causes */
#define E1000_ICR_LSC (1u << 2)    /* link status change */
#define E1000_ICR_RXDMT0 (1u << 4) /* receive ring running low */
#define E1000_ICR_RXO (1u << 6)    /* receive overrun */
#define E1000_ICR_RXT0 (1u << 7)   /* frame received */
#define E1000_IRQ_CAUSES                                                       \
  (E1000_ICR_LSC | E1000_ICR_RXDMT0 | E1000_ICR_RXO | E1000_ICR_RXT0)

#define E1000_EECD_AUTO_RD (1u << 9)

#define E1000_RAH_AV (1u << 31)

#define E1000_RCTL_EN (1u << 1)
#define E1000_RCTL_BAM (1u << 15)
#define E1000_RCTL_SECRC (1u << 26) /* strip the CRC */
/* BSIZE 00 with BSEX 0: 2048-byte buffers */

#define E1000_TCTL_EN (1u << 1)
#define E1000_TCTL_PSP (1u << 3)
#define E1000_TCTL_CT (0x0Fu << 4)
#define E1000_TCTL_COLD (0x3Fu << 12) /* full duplex */
#define E1000_TIPG_DEFAULT 0x0060200A

/* -- Descriptors ------------------------------------------ */

#define E1000_TX_CMD_EOP (1u << 0)
#define E1000_TX_CMD_IFCS (1u << 1)
#define E1000_TX_CMD_RS (1u << 3)
#define E1000_TX_STAT_DD (1u << 0)

#define E1000_RX_STAT_DD (1u << 0)
#define E1000_RX_STAT_EOP (1u << 1)

struct e1000_rx_desc {
  volatile uint64_t addr;
  volatile uint16_t length;
  volatile uint16_t checksum;
  volatile uint8_t status;
  volatile uint8_t errors;
  volatile uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
  volatile uint64_t addr;
  volatile uint16_t length;
  volatile uint8_t cso;
  volatile uint8_t cmd;
  volatile uint8_t status;
  volatile uint8_t css;
  volatile uint16_t special;
} __attribute__((packed));

/* -- Rings ------------------------------------------------ */

#define E1000_RX_DESCS 64 /* a multiple of 8: RDLEN is in 128-byte units */
#define E1000_TX_DESCS 64
#define E1000_BUF_SIZE 2048
#define E1000_MMIO_SIZE 0x20000
