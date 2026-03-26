#pragma once
#include <stdint.h>

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E // e1000 в QEMU

// Регистры e1000 (смещения от BAR0)
#define E1000_CTRL 0x0000
#define E1000_STATUS 0x0008
#define E1000_CTRL_RST (1 << 26)
#define E1000_CTRL_SLU (1 << 6) // set link up

#define E1000_RAL0 0x5400 // MAC адрес, младшие 4 байта
#define E1000_RAH0 0x5404 // MAC адрес, старшие 2 байта + valid bit

#define E1000_RDBAL 0x2800 // RX descriptor base low
#define E1000_RDBAH 0x2804 // RX descriptor base high
#define E1000_RDLEN 0x2808 // RX descriptor ring length (байты)
#define E1000_RDH 0x2810   // RX head
#define E1000_RDT 0x2818   // RX tail
#define E1000_RCTL 0x0100  // RX control

#define E1000_TDBAL 0x3800 // TX descriptor base low
#define E1000_TDBAH 0x3804 // TX descriptor base high
#define E1000_TDLEN 0x3808 // TX descriptor ring length
#define E1000_TDH 0x3810   // TX head
#define E1000_TDT 0x3818   // TX tail
#define E1000_TCTL 0x0400  // TX control

// RCTL bits
#define E1000_RCTL_EN (1 << 1)
#define E1000_RCTL_BAM (1 << 15) // broadcast accept
#define E1000_RCTL_BSIZE_2048 0	 // buffer size 2048

// TCTL bits
#define E1000_TCTL_EN (1 << 1)
#define E1000_TCTL_PSP (1 << 3) // pad short packets

#define E1000_RX_DESC_COUNT 32
#define E1000_TX_DESC_COUNT 32
#define E1000_BUFFER_SIZE 2048

// RX дескриптор
struct e1000_rx_desc
{
	volatile uint64_t addr;
	volatile uint16_t length;
	volatile uint16_t checksum;
	volatile uint8_t status;
	volatile uint8_t errors;
	volatile uint16_t special;
} __attribute__((packed));

// TX дескриптор
struct e1000_tx_desc
{
	volatile uint64_t addr;
	volatile uint16_t length;
	volatile uint8_t cso;
	volatile uint8_t cmd;
	volatile uint8_t status;
	volatile uint8_t css;
	volatile uint16_t special;
} __attribute__((packed));

#define E1000_TX_CMD_EOP (1 << 0) // end of packet
#define E1000_TX_CMD_RS (1 << 3)  // report status
#define E1000_TX_STAT_DD (1 << 0) // descriptor done

#define E1000_RX_STAT_DD (1 << 0)  // descriptor done
#define E1000_RX_STAT_EOP (1 << 1) // end of packet

int e1000_init(void);
int e1000_send(const void *data, uint16_t len);
int e1000_recv(void *buf, uint16_t *len_out);
void e1000_get_mac(uint8_t mac[6]);
