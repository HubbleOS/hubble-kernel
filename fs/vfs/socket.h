/**
 * @file socket.h
 * @brief Unix-domain stream sockets (AF_UNIX, SOCK_STREAM)
 *
 * A socket is a file_t. A connected pair shares two byte streams, one per
 * direction, and can pass open files along with the bytes (SCM_RIGHTS).
 * The syscall layer (sys_socket.c) translates addresses, flags and user
 * structures; everything here works on kernel memory and file_t.
 *
 * Names live in a kernel table, not in the filesystem: a bound path is
 * not visible to stat or ls, and it is released when the socket closes
 * (no stale socket file to unlink). Names starting with a NUL byte are
 * Linux's abstract names and work the same way.
 */
#pragma once

#include "file.h"
#include <stdbool.h>
#include <stddef.h>

/* sockaddr_un.sun_path: the longest name, with its terminator. */
#define UNIX_NAME_MAX 108

/* Files passed in one message (Linux allows 253). */
#define UNIX_MAX_RIGHTS 16

typedef struct {
  file_t *files[UNIX_MAX_RIGHTS];
  int count;
} unix_rights_t;

/** A new unconnected socket (one reference). */
file_t *unix_socket_create(void);

/** Two sockets connected to each other (socketpair). */
int unix_socketpair(file_t **a, file_t **b);

/** Whether @p file is a socket (else the socket calls say -ENOTSOCK). */
bool unix_is_socket(file_t *file);

/** Give the socket a name: a path, or an abstract name (leading NUL). */
long unix_bind(file_t *sock, const char *name, size_t len);

/** Accept connections, queueing up to @p backlog of them. */
long unix_listen(file_t *sock, int backlog);

/** The next queued connection, as a new socket (one reference). */
long unix_accept(file_t *sock, file_t **conn);

/** Connect to the listening socket bound to @p name. */
long unix_connect(file_t *sock, const char *name, size_t len);

/**
 * Send @p len bytes, and with them the files in @p rights (may be NULL),
 * whose references are passed along on success. The files arrive with
 * the first byte. Returns bytes sent or -errno.
 */
long unix_send(file_t *sock, const void *buf, size_t len,
               unix_rights_t *rights, bool nonblock);

/**
 * Receive up to @p len bytes. A read never spans two messages that carry
 * files. With @p rights, files that arrive with the first byte read are
 * moved there (the caller owns their references); without, they are
 * closed, as for read(2). @p continuing reads on after earlier bytes of
 * the same call: it never waits and stops at the next files.
 */
long unix_recv(file_t *sock, void *buf, size_t len, unix_rights_t *rights,
               bool nonblock, bool continuing);

/** The pid of the process at the other end, as it was when the
 * connection was made (SO_PEERCRED), or -ENOTCONN. */
long unix_peer_pid(file_t *sock);

/** shutdown(2): @p read and/or @p write side. */
long unix_shutdown(file_t *sock, bool read, bool write);
