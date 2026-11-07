/*
 * Network sockets: AF_INET (TCP, UDP, raw ICMP), AF_PACKET and AF_NETLINK
 * (NETLINK_ROUTE), as files.
 *
 * The syscall layer (arch/x86/kernel/syscalls/sys_socket.c) checks the
 * descriptor and the flags it knows, and calls these for any socket that
 * net_is_socket(); addresses and option values are the user's Linux
 * structures (sockaddr_in, sockaddr_ll, timeval, ...), read and written
 * here. Errors are -errno.
 */
#pragma once

#include <fs/vfs/file.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NET_AF_INET 2
#define NET_AF_INET6 10
#define NET_AF_NETLINK 16
#define NET_AF_PACKET 17

/* The same layout as struct iovec. */
struct net_iov {
  void *base;
  size_t len;
};

/* recvmsg's msg_flags */
#define NET_MSG_TRUNC 0x20

/* send/recv flags understood here */
#define NET_MSG_PEEK 0x2
#define NET_MSG_DONTWAIT 0x40
#define NET_MSG_WAITALL 0x100
#define NET_MSG_NOSIGNAL 0x4000
#define NET_MSG_MORE 0x8000

/** A new socket (one reference); @p type without SOCK_NONBLOCK/CLOEXEC. */
long net_socket(int domain, int type, int protocol, file_t **out);
bool net_is_socket(file_t *file);

long net_bind(file_t *sock, const void *addr, uint32_t len);
long net_listen(file_t *sock, int backlog);
/** The next connection as a new socket; its address in @p addr / @p len
 * (either may be NULL). */
long net_accept(file_t *sock, file_t **conn, void *addr, uint32_t *len);
long net_connect(file_t *sock, const void *addr, uint32_t len);
long net_shutdown(file_t *sock, int how);

/** Send the bytes of @p iov, to @p addr if given. @return bytes sent */
long net_send(file_t *sock, const struct net_iov *iov, size_t iovlen,
              const void *addr, uint32_t addrlen, int flags);
/** Receive into @p iov; the sender's address goes to @p addr / @p addrlen
 * and NET_MSG_TRUNC to @p msg_flags (each may be NULL). @return bytes */
long net_recv(file_t *sock, const struct net_iov *iov, size_t iovlen,
              void *addr, uint32_t *addrlen, int flags, int *msg_flags);

/** ioctl(2) of the SIOC* interface and routing requests, from any socket.
 * -ENOTTY if @p request is not one. */
long net_if_ioctl(unsigned long request, void *arg);

long net_getsockopt(file_t *sock, int level, int name, void *val,
                    uint32_t *len);
long net_setsockopt(file_t *sock, int level, int name, const void *val,
                    uint32_t len);
/** getsockname (@p peer false) or getpeername. */
long net_getname(file_t *sock, void *addr, uint32_t *len, bool peer);
