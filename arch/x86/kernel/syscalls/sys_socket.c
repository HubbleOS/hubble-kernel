/*
 * Socket syscalls (Linux x86_64 ABI) for Unix-domain stream sockets, and
 * memfd_create. The socket logic is in fs/vfs/socket.c; this file reads
 * and writes the user's structures (sockaddr_un, msghdr, cmsghdr) and
 * maps flags.
 */
#include <hubble/errno.h>
#include <hubble/string.h>
#include <hubble/syscalls.h>

#include <fs/vfs/file.h>
#include <fs/vfs/socket.h>
#include <smp/fdtable.h>
#include <smp/scheduler.h>
#include <smp/task.h>

#include "syscall_entry.h"

#define AF_UNIX 1
#define SOCK_STREAM 1
#define SOCK_TYPE_MASK 0xf
#define SOCK_NONBLOCK 0x800
#define SOCK_CLOEXEC 0x80000

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

#define MSG_DONTWAIT 0x40
#define MSG_CTRUNC 0x8
#define MSG_WAITALL 0x100
#define MSG_NOSIGNAL 0x4000 /* no signals here: EPIPE is all there is */
#define MSG_MORE 0x8000     /* a hint; every send goes out at once */
#define MSG_CMSG_CLOEXEC 0x40000000

#define SOL_SOCKET 1
#define SCM_RIGHTS 1

#define UIO_MAXIOV 1024

#define MFD_CLOEXEC 0x1
#define MFD_ALLOW_SEALING 0x2 /* accepted; no seals are ever applied */
#define MFD_NAME_MAX 249

typedef uint32_t socklen_t;

struct sockaddr_un {
  uint16_t sun_family;
  char sun_path[UNIX_NAME_MAX];
};

struct msghdr {
  void *msg_name;
  socklen_t msg_namelen;
  struct iovec *msg_iov;
  size_t msg_iovlen;
  void *msg_control;
  size_t msg_controllen;
  int msg_flags;
};

struct cmsghdr {
  size_t cmsg_len;
  int cmsg_level;
  int cmsg_type;
  /* followed by the data */
};

#define CMSG_ALIGN(len) (((len) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#define CMSG_HDR_LEN CMSG_ALIGN(sizeof(struct cmsghdr))

/* -- Helpers ---------------------------------------------- */

/* The socket behind @p fd. */
static long sock_file(int fd, file_t **out) {
  file_t *file = fd_file(get_current_task(), fd);
  if (!file)
    return -EBADF;
  if (!unix_is_socket(file))
    return -ENOTSOCK;
  *out = file;
  return 0;
}

/* Give a new socket a descriptor (taking over its reference). */
static long install(file_t *file, int flags) {
  if (flags & SOCK_NONBLOCK)
    file->flags |= FILE_O_NONBLOCK;
  int fd = fd_install(get_current_task(), file, 0,
                      (flags & SOCK_CLOEXEC) ? FD_CLOEXEC : 0);
  if (fd < 0)
    file_put(file);
  return fd;
}

/* The socket name in a sockaddr_un: an absolute path (relative ones
 * resolve against the cwd, so both ends agree), or an abstract name,
 * which starts with a NUL byte. Returns its length or -errno. */
static long unix_name(const struct sockaddr_un *addr, socklen_t len,
                      char name[UNIX_NAME_MAX]) {
  if (!addr)
    return -EFAULT;
  if (len <= sizeof(addr->sun_family) || len > sizeof(*addr))
    return -EINVAL;
  if (addr->sun_family != AF_UNIX)
    return -EAFNOSUPPORT;
  size_t n = len - sizeof(addr->sun_family);

  if (addr->sun_path[0] == '\0') {
    memcpy(name, addr->sun_path, n);
    return (long)n;
  }

  char path[UNIX_NAME_MAX + 1];
  memcpy(path, addr->sun_path, n);
  path[n] = '\0'; /* the path may fill sun_path without a terminator */
  char abs[SYS_PATH_MAX];
  if (!sys_abs_path(path, abs, sizeof(abs)))
    return -ENAMETOOLONG;
  size_t abs_len = strlen(abs);
  if (abs_len > UNIX_NAME_MAX)
    return -ENAMETOOLONG;
  memcpy(name, abs, abs_len);
  return (long)abs_len;
}

/* The address of a connection's other end: unnamed (just the family). */
static void put_peer_addr(struct sockaddr_un *addr, socklen_t *len) {
  if (!addr || !len)
    return;
  if (*len >= sizeof(addr->sun_family))
    addr->sun_family = AF_UNIX;
  *len = sizeof(addr->sun_family);
}

/* -- Creating and naming ---------------------------------- */

static long check_type(int domain, int type, int protocol) {
  if (domain != AF_UNIX)
    return -EAFNOSUPPORT;
  if ((type & SOCK_TYPE_MASK) != SOCK_STREAM)
    return -ESOCKTNOSUPPORT;
  if (type & ~(SOCK_TYPE_MASK | SOCK_NONBLOCK | SOCK_CLOEXEC))
    return -EINVAL;
  if (protocol != 0)
    return -EPROTONOSUPPORT;
  return 0;
}

long sys_socket(int domain, int type, int protocol) {
  long err = check_type(domain, type, protocol);
  if (err)
    return err;
  file_t *file = unix_socket_create();
  if (!file)
    return -ENOMEM;
  return install(file, type);
}

long sys_socketpair(int domain, int type, int protocol, int sv[2]) {
  long err = check_type(domain, type, protocol);
  if (err)
    return err;
  if (!sv)
    return -EFAULT;
  file_t *a, *b;
  err = unix_socketpair(&a, &b);
  if (err)
    return err;

  long fa = install(a, type);
  if (fa < 0) {
    file_put(b);
    return fa;
  }
  long fb = install(b, type);
  if (fb < 0) {
    fd_close(get_current_task(), (int)fa);
    return fb;
  }
  sv[0] = (int)fa;
  sv[1] = (int)fb;
  return 0;
}

long sys_bind(int fd, const struct sockaddr_un *addr, socklen_t len) {
  file_t *file;
  long err = sock_file(fd, &file);
  if (err)
    return err;
  char name[UNIX_NAME_MAX];
  long n = unix_name(addr, len, name);
  if (n < 0)
    return n;
  return unix_bind(file, name, (size_t)n);
}

long sys_listen(int fd, int backlog) {
  file_t *file;
  long err = sock_file(fd, &file);
  return err ? err : unix_listen(file, backlog);
}

long sys_accept4(int fd, struct sockaddr_un *addr, socklen_t *len,
                 int flags) {
  if (flags & ~(SOCK_NONBLOCK | SOCK_CLOEXEC))
    return -EINVAL;
  file_t *file;
  long err = sock_file(fd, &file);
  if (err)
    return err;
  file_t *conn;
  err = unix_accept(file, &conn);
  if (err)
    return err;
  long newfd = install(conn, flags);
  if (newfd >= 0)
    put_peer_addr(addr, len);
  return newfd;
}

long sys_accept(int fd, struct sockaddr_un *addr, socklen_t *len) {
  return sys_accept4(fd, addr, len, 0);
}

long sys_connect(int fd, const struct sockaddr_un *addr, socklen_t len) {
  file_t *file;
  long err = sock_file(fd, &file);
  if (err)
    return err;
  char name[UNIX_NAME_MAX];
  long n = unix_name(addr, len, name);
  if (n < 0)
    return n;
  return unix_connect(file, name, (size_t)n);
}

long sys_shutdown(int fd, int how) {
  file_t *file;
  long err = sock_file(fd, &file);
  if (err)
    return err;
  if (how != SHUT_RD && how != SHUT_WR && how != SHUT_RDWR)
    return -EINVAL;
  return unix_shutdown(file, how != SHUT_WR, how != SHUT_RD);
}

/* -- Sending ---------------------------------------------- */

static bool send_nonblock(file_t *file, int flags) {
  return (file->flags & FILE_O_NONBLOCK) || (flags & MSG_DONTWAIT);
}

long sys_sendto(int fd, const void *buf, size_t len, int flags,
                const struct sockaddr_un *dest, socklen_t dest_len) {
  (void)dest_len;
  file_t *file;
  long err = sock_file(fd, &file);
  if (err)
    return err;
  if (flags & ~(MSG_DONTWAIT | MSG_NOSIGNAL | MSG_MORE))
    return -EOPNOTSUPP;
  if (dest)
    return -EISCONN; /* stream sockets send to their peer only */
  return unix_send(file, buf, len, NULL, send_nonblock(file, flags));
}

/* Collect the files of SCM_RIGHTS messages in @p msg's control data,
 * taking a reference to each. */
static long take_rights(const struct msghdr *msg, unix_rights_t *rights) {
  const uint8_t *control = msg->msg_control;
  size_t len = control ? msg->msg_controllen : 0;
  task_t *task = get_current_task();

  for (size_t off = 0; off + CMSG_HDR_LEN <= len;) {
    const struct cmsghdr *c = (const struct cmsghdr *)(control + off);
    if (c->cmsg_len < CMSG_HDR_LEN || c->cmsg_len > len - off)
      return -EINVAL;
    if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS)
      return -EINVAL; /* e.g. SCM_CREDENTIALS: not supported */

    const int *fds = (const int *)((const uint8_t *)c + CMSG_HDR_LEN);
    size_t n = (c->cmsg_len - CMSG_HDR_LEN) / sizeof(int);
    for (size_t i = 0; i < n; i++) {
      if (rights->count == UNIX_MAX_RIGHTS)
        return -ETOOMANYREFS;
      file_t *file = fd_file(task, fds[i]);
      if (!file)
        return -EBADF;
      rights->files[rights->count++] = file_get(file);
    }
    off += CMSG_ALIGN(c->cmsg_len);
  }
  return 0;
}

static void put_rights(unix_rights_t *rights) {
  for (int i = 0; i < rights->count; i++)
    file_put(rights->files[i]);
  rights->count = 0;
}

long sys_sendmsg(int fd, const struct msghdr *msg, int flags) {
  file_t *file;
  long err = sock_file(fd, &file);
  if (err)
    return err;
  if (!msg || (msg->msg_iovlen && !msg->msg_iov))
    return -EFAULT;
  if (msg->msg_iovlen > UIO_MAXIOV)
    return -EMSGSIZE;
  if (flags & ~(MSG_DONTWAIT | MSG_NOSIGNAL | MSG_MORE))
    return -EOPNOTSUPP;
  if (msg->msg_name)
    return -EISCONN;

  unix_rights_t rights = {0};
  err = take_rights(msg, &rights);
  if (err) {
    put_rights(&rights);
    return err;
  }

  /* The files go with the first bytes sent. */
  bool nb = send_nonblock(file, flags);
  long total = 0;
  for (size_t i = 0; i < msg->msg_iovlen; i++) {
    const struct iovec *iov = &msg->msg_iov[i];
    if (!iov->iov_len)
      continue;
    long n = unix_send(file, iov->iov_base, iov->iov_len, &rights, nb);
    if (n < 0) {
      if (!total)
        total = n;
      break;
    }
    total += n;
    if ((size_t)n < iov->iov_len)
      break;
  }
  put_rights(&rights); /* only left if nothing was sent */
  return total;
}

/* -- Receiving -------------------------------------------- */

static long check_recv_flags(int flags) {
  if (flags & ~(MSG_DONTWAIT | MSG_WAITALL | MSG_NOSIGNAL | MSG_CMSG_CLOEXEC))
    return -EOPNOTSUPP; /* MSG_PEEK, MSG_OOB, ... */
  return 0;
}

long sys_recvfrom(int fd, void *buf, size_t len, int flags,
                  struct sockaddr_un *src, socklen_t *src_len) {
  file_t *file;
  long err = sock_file(fd, &file);
  if (err)
    return err;
  err = check_recv_flags(flags);
  if (err)
    return err;

  bool nb = send_nonblock(file, flags);
  long total = unix_recv(file, buf, len, NULL, nb, false);
  /* MSG_WAITALL: keep reading until full or end of file. */
  while ((flags & MSG_WAITALL) && total > 0 && (size_t)total < len) {
    long n = unix_recv(file, (uint8_t *)buf + total, len - total, NULL, nb,
                       false);
    if (n <= 0)
      break;
    total += n;
  }
  if (total >= 0)
    put_peer_addr(src, src_len);
  return total;
}

/* Install received files as descriptors and describe them in @p msg's
 * control buffer; what doesn't fit is closed and flagged MSG_CTRUNC. */
static void deliver_rights(struct msghdr *msg, unix_rights_t *rights,
                           int flags) {
  size_t space = msg->msg_control ? msg->msg_controllen : 0;
  size_t fit = space >= CMSG_HDR_LEN ? (space - CMSG_HDR_LEN) / sizeof(int)
                                     : 0;
  struct cmsghdr *c = msg->msg_control;
  int *fds = (int *)((uint8_t *)c + CMSG_HDR_LEN);
  task_t *task = get_current_task();

  int installed = 0;
  for (int i = 0; i < rights->count; i++) {
    int fd = -1;
    if ((size_t)installed < fit)
      fd = fd_install(task, rights->files[i], 0,
                      (flags & MSG_CMSG_CLOEXEC) ? FD_CLOEXEC : 0);
    if (fd < 0) {
      file_put(rights->files[i]);
      msg->msg_flags |= MSG_CTRUNC;
      continue;
    }
    fds[installed++] = fd;
  }
  rights->count = 0;

  if (!installed) {
    msg->msg_controllen = 0;
    return;
  }
  c->cmsg_len = CMSG_HDR_LEN + installed * sizeof(int);
  c->cmsg_level = SOL_SOCKET;
  c->cmsg_type = SCM_RIGHTS;
  size_t used = CMSG_ALIGN(c->cmsg_len);
  msg->msg_controllen = used < space ? used : space;
}

long sys_recvmsg(int fd, struct msghdr *msg, int flags) {
  file_t *file;
  long err = sock_file(fd, &file);
  if (err)
    return err;
  if (!msg || (msg->msg_iovlen && !msg->msg_iov))
    return -EFAULT;
  if (msg->msg_iovlen > UIO_MAXIOV)
    return -EMSGSIZE;
  err = check_recv_flags(flags);
  if (err)
    return err;

  /* The first read may wait; the rest only take what is already there,
   * and stop where another message's files begin. */
  bool nb = send_nonblock(file, flags);
  unix_rights_t rights = {0};
  long total = 0;
  for (size_t i = 0; i < msg->msg_iovlen; i++) {
    struct iovec *iov = &msg->msg_iov[i];
    if (!iov->iov_len)
      continue;
    long n = unix_recv(file, iov->iov_base, iov->iov_len, &rights, nb,
                       total > 0);
    if (n < 0) {
      if (!total)
        total = n;
      break;
    }
    total += n;
    if ((size_t)n < iov->iov_len)
      break;
  }

  msg->msg_flags = 0;
  msg->msg_namelen = 0; /* the peer is unnamed */
  deliver_rights(msg, &rights, flags);
  return total;
}

/* -- Socket options --------------------------------------- */

#define SO_TYPE 3
#define SO_ERROR 4
#define SO_SNDBUF 7
#define SO_RCVBUF 8
#define SO_PEERCRED 17

struct ucred {
  int32_t pid;
  uint32_t uid;
  uint32_t gid;
};

static long put_int_opt(void *val, socklen_t *len, int v) {
  if (!val || !len)
    return -EFAULT;
  if (*len < sizeof(int))
    return -EINVAL;
  memcpy(val, &v, sizeof(int));
  *len = sizeof(int);
  return 0;
}

long sys_getsockopt(int fd, int level, int name, void *val, socklen_t *len) {
  file_t *file;
  long err = sock_file(fd, &file);
  if (err)
    return err;
  if (level != SOL_SOCKET)
    return -ENOPROTOOPT;
  switch (name) {
  case SO_TYPE:
    return put_int_opt(val, len, SOCK_STREAM);
  case SO_ERROR:
    return put_int_opt(val, len, 0);
  case SO_SNDBUF:
  case SO_RCVBUF:
    return put_int_opt(val, len, 64 * 1024);
  case SO_PEERCRED: {
    if (!val || !len)
      return -EFAULT;
    if (*len < sizeof(struct ucred))
      return -EINVAL;
    long pid = unix_peer_pid(file);
    if (pid < 0)
      return pid;
    struct ucred cred = {(int32_t)pid, 0, 0}; /* everyone is root */
    memcpy(val, &cred, sizeof(cred));
    *len = sizeof(cred);
    return 0;
  }
  default:
    return -ENOPROTOOPT;
  }
}

/* Buffer sizes are fixed; asking for others is accepted, as on Linux,
 * which only treats them as hints. */
long sys_setsockopt(int fd, int level, int name, const void *val,
                    socklen_t len) {
  (void)val, (void)len;
  file_t *file;
  long err = sock_file(fd, &file);
  if (err)
    return err;
  if (level == SOL_SOCKET && (name == SO_SNDBUF || name == SO_RCVBUF))
    return 0;
  return -ENOPROTOOPT;
}

/* -- memfd ------------------------------------------------ */

long sys_memfd_create(const char *name, unsigned int flags) {
  if (!name)
    return -EFAULT;
  if (flags & ~(MFD_CLOEXEC | MFD_ALLOW_SEALING))
    return -EINVAL;
  /* The name is only a label (Linux shows it in /proc/<pid>/fd). */
  size_t n = 0;
  while (name[n] && n <= MFD_NAME_MAX)
    n++;
  if (n > MFD_NAME_MAX)
    return -EINVAL;

  file_t *file = memfd_create_file();
  if (!file)
    return -ENOMEM;
  int fd = fd_install(get_current_task(), file, 0,
                      (flags & MFD_CLOEXEC) ? FD_CLOEXEC : 0);
  if (fd < 0)
    file_put(file);
  return fd;
}
