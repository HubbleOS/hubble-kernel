/**
 * @file ipv4.h
 * @brief IPv4 protocol definitions
 */

#pragma once

#include <stdint.h>

/** @brief IP protocol number for UDP */
#define IP_PROTO_UDP 17

/**
 * @brief IPv4 header structure
 */
struct ip_hdr {
  uint8_t ihl_ver;   /**< Version (4) and IHL (5) combined */
  uint8_t tos;       /**< Type of service */
  uint16_t tot_len;  /**< Total length */
  uint16_t id;       /**< Identification */
  uint16_t frag_off; /**< Fragment offset and flags */
  uint8_t ttl;       /**< Time to live */
  uint8_t proto;     /**< Protocol */
  uint16_t checksum; /**< Header checksum */
  uint32_t src;      /**< Source IP address */
  uint32_t dst;      /**< Destination IP address */
} __attribute__((packed));

/**
 * @brief Compute the IP header checksum
 * @param data Pointer to the IP header
 * @param len Length of the header (typically 20 bytes)
 * @return 16-bit ones' complement checksum
 */
uint16_t ip_checksum(const void *data, uint16_t len);

/**
 * @brief Send an IPv4 packet
 * @param dst_ip Destination IP address
 * @param proto IP protocol number
 * @param payload Payload data
 * @param len Payload length
 */
void ip_send(uint32_t dst_ip, uint8_t proto, const void *payload, uint16_t len);
