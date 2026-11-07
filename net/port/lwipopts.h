/*
 * lwIP configuration for the kernel: the raw API only (NO_SYS), driven
 * by the net thread and the socket syscalls under one lock (net/core.c).
 * The Linux socket API is ours (net/socket.c), DHCP is userspace
 * (udhcpc), so lwIP's sockets, netconn, DHCP and DNS are off.
 */
#pragma once

#define NO_SYS 1
#define SYS_LIGHTWEIGHT_PROT 0
#define LWIP_TIMERS 1
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0
#define LWIP_NETIF_API 0

#define LWIP_IPV4 1
#define LWIP_IPV6 0
#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_DHCP 0
#define LWIP_AUTOIP 0
#define LWIP_ACD 0
#define LWIP_IGMP 0
#define LWIP_DNS 0
#define IP_FORWARD 0
#define IP_REASSEMBLY 1
#define IP_FRAG 1
#define IP_SOF_BROADCAST 0
#define LWIP_BROADCAST_PING 1

/* lo, 127.0.0.1 */
#define LWIP_HAVE_LOOPIF 1
#define LWIP_NETIF_LOOPBACK 1
#define LWIP_LOOPBACK_MAX_PBUFS 128
#define LWIP_SINGLE_NETIF 0

/* Memory: lwIP's own heap and pools, static, so nothing in lwIP calls
 * kmalloc with the net lock held. */
#define MEM_LIBC_MALLOC 0
#define MEMP_MEM_MALLOC 0
#define MEM_ALIGNMENT 8
#define MEM_SIZE (2 * 1024 * 1024)
#define MEMP_NUM_PBUF 256
#define MEMP_NUM_RAW_PCB 32
#define MEMP_NUM_UDP_PCB 64
#define MEMP_NUM_TCP_PCB 128
#define MEMP_NUM_TCP_PCB_LISTEN 32
#define MEMP_NUM_TCP_SEG 512
#define MEMP_NUM_REASSDATA 8
#define IP_REASS_MAX_PBUFS 32
#define MEMP_NUM_FRAG_PBUF 32
#define MEMP_NUM_ARP_QUEUE 64
#define PBUF_POOL_SIZE 512

/* TCP: 1460-byte segments, 45 KB windows. */
#define TCP_MSS 1460
#define TCP_WND (31 * TCP_MSS)
#define TCP_SND_BUF (31 * TCP_MSS)
#define TCP_SND_QUEUELEN (4 * TCP_SND_BUF / TCP_MSS)
#define TCP_QUEUE_OOSEQ 1
#define TCP_LISTEN_BACKLOG 1
#define TCP_OVERSIZE TCP_MSS
#define LWIP_TCP_KEEPALIVE 1
#define LWIP_TCP_TIMESTAMPS 0
#define SO_REUSE 1

#define ARP_QUEUEING 1
#define ETH_PAD_SIZE 0
#define LWIP_STATS 0
#define LWIP_NETIF_STATUS_CALLBACK 0
#define LWIP_NETIF_LINK_CALLBACK 0
#define LWIP_NETIF_HOSTNAME 0
#define LWIP_CHECKSUM_CTRL_PER_NETIF 0
