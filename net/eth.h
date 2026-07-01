/**
 * @file eth.h
 * @brief Ethernet protocol definitions
 */

#pragma once

#include <stdint.h>

/** @brief Ethernet type for ARP */
#define ETH_TYPE_ARP 0x0806
/** @brief Ethernet type for IPv4 */
#define ETH_TYPE_IP 0x0800

/**
 * @brief Send an Ethernet frame
 * @param dst Destination MAC address (6 bytes)
 * @param type Ethernet type (host byte order)
 * @param payload Payload data
 * @param len Payload length
 */
void eth_send(uint8_t dst[6], uint16_t type, const void *payload, uint16_t len);

/**
 * @brief Receive an Ethernet frame
 * @param payload_out Buffer for the payload data
 * @param len_out Length of the received payload
 * @param type_out Ethernet type of the received frame
 * @return 0 on success, -1 on error or if an ARP packet was handled internally
 */
int eth_recv(uint8_t *payload_out, uint16_t *len_out, uint16_t *type_out);
