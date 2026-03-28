#pragma once
#include <stdint.h>

#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IP 0x0800

void eth_send(uint8_t dst[6], uint16_t type, const void *payload, uint16_t len);
int eth_recv(uint8_t *payload_out, uint16_t *len_out, uint16_t *type_out);
