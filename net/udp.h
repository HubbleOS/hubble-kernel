/**
 * @file udp.h
 * @brief UDP protocol definitions
 */

#pragma once

#include <stdint.h>

/**
 * @brief UDP header structure
 */
struct udp_hdr {
  uint16_t src_port; /**< Source port */
  uint16_t dst_port; /**< Destination port */
  uint16_t length;   /**< Length (header + data) */
  uint16_t checksum; /**< Checksum (optional for UDP) */
} __attribute__((packed));

/**
 * @brief Send a UDP datagram
 * @param dst_ip Destination IP address
 * @param src_port Source port
 * @param dst_port Destination port
 * @param data Payload data
 * @param len Payload length
 */
void udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
              const void *data, uint16_t len);

/**
 * @brief Receive a UDP datagram on a given port
 * @param port Local port to listen on
 * @param buf_out Buffer for the payload data
 * @param len_out Length of the received payload
 * @return 0 on success, -1 on error
 */
int udp_recv(uint16_t port, void *buf_out, uint16_t *len_out);
