/**
 * @file arp.h
 * @brief ARP (Address Resolution Protocol) definitions
 */

#pragma once

#include <stdint.h>

/** @brief Ethernet type for ARP */
#define ETH_TYPE_ARP 0x0806
/** @brief Ethernet type for IPv4 */
#define ETH_TYPE_IP  0x0800

/** @brief ARP request operation code */
#define ARP_REQUEST 1
/** @brief ARP reply operation code */
#define ARP_REPLY   2

/**
 * @brief ARP packet structure (Ethernet + IPv4)
 */
struct arp_pkt
{
	uint16_t htype; /**< Hardware type (1 = Ethernet) */
	uint16_t ptype; /**< Protocol type (0x0800 = IP) */
	uint8_t  hlen;  /**< Hardware address length (6) */
	uint8_t  plen;  /**< Protocol address length (4) */
	uint16_t oper;  /**< Operation: 1 = request, 2 = reply */
	uint8_t  sha[6]; /**< Sender MAC address */
	uint32_t spa;   /**< Sender IP address */
	uint8_t  tha[6]; /**< Target MAC address */
	uint32_t tpa;   /**< Target IP address */
} __attribute__((packed));

/**
 * @brief Build a 32-bit IP address from four octets
 * @param a First octet (MSB)
 * @param b Second octet
 * @param c Third octet
 * @param d Fourth octet (LSB)
 */
#define ARP_IP(a, b, c, d) \
	((uint32_t)(a) | ((uint32_t)(b) << 8) | \
	 ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

/**
 * @brief Send an ARP request for a given IP
 * @param target_ip Target IP address (network byte order)
 * @return 0 on success, -1 on error
 */
int arp_request(uint32_t target_ip);

/**
 * @brief Look up a MAC address for an IP in the ARP cache
 * @param ip IP address to look up
 * @param mac_out Buffer for the MAC address (6 bytes)
 * @return 0 on success, -1 if not found
 */
int arp_lookup(uint32_t ip, uint8_t mac_out[6]);

/**
 * @brief Handle an incoming ARP packet
 * @param pkt Pointer to the ARP packet data
 * @param len Length of the ARP packet
 */
void arp_handle(const uint8_t *pkt, uint16_t len);
