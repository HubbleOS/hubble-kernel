/**
 * @file udp.c
 * @brief UDP protocol implementation
 *
 * Provides UDP datagram transmission and reception over IPv4.
 */

#include "udp.h"
#include "ipv4.h"
#include <hubble/printk.h>
#include <hubble/string.h>
#include <mm/kmalloc.h>
#include <net/eth.h>

/**
 * @brief Send a UDP datagram
 * @param dst_ip Destination IP address
 * @param src_port Source port
 * @param dst_port Destination port
 * @param data Payload data
 * @param len Payload length
 */
void udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port,
              const void *data, uint16_t len) {
  uint16_t udp_len = sizeof(struct udp_hdr) + len;
  uint8_t *buf = kmalloc(udp_len, GFP_KERNEL);

  struct udp_hdr *hdr = (struct udp_hdr *)buf;
  hdr->src_port = __builtin_bswap16(src_port);
  hdr->dst_port = __builtin_bswap16(dst_port);
  hdr->length = __builtin_bswap16(udp_len);
  hdr->checksum = 0;

  memcpy(buf + sizeof(struct udp_hdr), data, len);

  ip_send(dst_ip, IP_PROTO_UDP, buf, udp_len);
  kfree(buf);
}

/**
 * @brief Receive a UDP datagram on a given port
 * @param port Local port to listen on
 * @param buf_out Buffer for the payload data
 * @param len_out Length of the received payload
 * @return 0 on success, -1 on error
 */
int udp_recv(uint16_t port, void *buf_out, uint16_t *len_out) {
  uint8_t buf[1500];
  uint16_t len;

  if (eth_recv(buf, &len, NULL) != 0)
    return -1;

  struct ip_hdr *ip = (struct ip_hdr *)buf;
  if (ip->proto != IP_PROTO_UDP)
    return -1;

  struct udp_hdr *udp = (struct udp_hdr *)(buf + sizeof(struct ip_hdr));
  if (__builtin_bswap16(udp->dst_port) != port)
    return -1;

  uint16_t data_len = __builtin_bswap16(udp->length) - sizeof(struct udp_hdr);
  memcpy(buf_out, (uint8_t *)udp + sizeof(struct udp_hdr), data_len);
  *len_out = data_len;
  return 0;
}
