/*
 * socktest - checks Unix sockets, fd passing and memfd shared memory.
 *
 * Every check states what Linux does, so the same binary is the
 * reference on a Linux host and the test on HubbleOS:
 *
 *   socktest      prints each failed check, then "N passed, M failed"
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int passed, failed;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (cond) {                                                                \
      passed++;                                                                \
    } else {                                                                   \
      failed++;                                                                \
      printf("FAIL %s:%d: %s (errno %d)\n", __func__, __LINE__, #cond,         \
             errno);                                                           \
    }                                                                          \
  } while (0)

static long now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void sleep_ms(long ms) {
  struct timespec ts = {ms / 1000, (ms % 1000) * 1000000};
  nanosleep(&ts, NULL);
}

/* Send @p len bytes and, if @p fd >= 0, that descriptor with them. */
static ssize_t send_fd(int sock, const void *buf, size_t len, int fd) {
  struct iovec iov = {(void *)buf, len};
  union {
    struct cmsghdr h;
    char space[CMSG_SPACE(sizeof(int))];
  } ctl;
  struct msghdr msg = {.msg_iov = &iov, .msg_iovlen = 1};
  if (fd >= 0) {
    msg.msg_control = ctl.space;
    msg.msg_controllen = sizeof(ctl.space);
    struct cmsghdr *c = CMSG_FIRSTHDR(&msg);
    c->cmsg_level = SOL_SOCKET;
    c->cmsg_type = SCM_RIGHTS;
    c->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(c), &fd, sizeof(int));
  }
  return sendmsg(sock, &msg, MSG_NOSIGNAL);
}

/* Receive into @p buf; *fd gets a passed descriptor, or -1. */
static ssize_t recv_fd(int sock, void *buf, size_t len, int *fd) {
  struct iovec iov = {buf, len};
  union {
    struct cmsghdr h;
    char space[CMSG_SPACE(sizeof(int) * 4)];
  } ctl;
  struct msghdr msg = {.msg_iov = &iov,
                       .msg_iovlen = 1,
                       .msg_control = ctl.space,
                       .msg_controllen = sizeof(ctl.space)};
  *fd = -1;
  ssize_t n = recvmsg(sock, &msg, 0);
  for (struct cmsghdr *c = CMSG_FIRSTHDR(&msg); n >= 0 && c;
       c = CMSG_NXTHDR(&msg, c))
    if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS)
      memcpy(fd, CMSG_DATA(c), sizeof(int));
  return n;
}

static void test_socketpair(void) {
  int sv[2];
  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  char buf[16] = {0};
  CHECK(write(sv[0], "hello", 5) == 5);
  CHECK(read(sv[1], buf, sizeof(buf)) == 5 && memcmp(buf, "hello", 5) == 0);
  CHECK(send(sv[1], "back", 4, 0) == 4);
  CHECK(recv(sv[0], buf, sizeof(buf), 0) == 4 && memcmp(buf, "back", 4) == 0);

  struct stat st;
  CHECK(fstat(sv[0], &st) == 0 && S_ISSOCK(st.st_mode));

  /* Nothing queued: a non-blocking read would block. */
  CHECK(recv(sv[0], buf, sizeof(buf), MSG_DONTWAIT) == -1 && errno == EAGAIN);

  /* Half close: the peer reads EOF but can still send back. */
  CHECK(shutdown(sv[0], SHUT_WR) == 0);
  CHECK(read(sv[1], buf, sizeof(buf)) == 0);
  CHECK(write(sv[1], "x", 1) == 1);
  CHECK(read(sv[0], buf, sizeof(buf)) == 1);

  /* Closed peer: EOF on reading, EPIPE on writing. */
  close(sv[1]);
  CHECK(read(sv[0], buf, sizeof(buf)) == 0);
  CHECK(send(sv[0], "x", 1, MSG_NOSIGNAL) == -1 && errno == EPIPE);
  close(sv[0]);

  /* No network on HubbleOS: other families are refused cleanly. */
  int inet = socket(AF_INET, SOCK_STREAM, 0);
  CHECK(inet >= 0 || errno == EAFNOSUPPORT);
  if (inet >= 0)
    close(inet);
}

static void test_listen_connect(void) {
  /* An abstract name: no file to clean up on Linux. */
  struct sockaddr_un addr = {.sun_family = AF_UNIX};
  memcpy(addr.sun_path, "\0socktest", 9);
  socklen_t len = offsetof(struct sockaddr_un, sun_path) + 9;

  int c = socket(AF_UNIX, SOCK_STREAM, 0);
  CHECK(connect(c, (struct sockaddr *)&addr, len) == -1 &&
        errno == ECONNREFUSED);

  int l = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
  CHECK(bind(l, (struct sockaddr *)&addr, len) == 0);
  int l2 = socket(AF_UNIX, SOCK_STREAM, 0);
  CHECK(bind(l2, (struct sockaddr *)&addr, len) == -1 && errno == EADDRINUSE);
  close(l2);
  CHECK(listen(l, 4) == 0);
  CHECK(accept(l, NULL, NULL) == -1 && errno == EAGAIN);

  CHECK(connect(c, (struct sockaddr *)&addr, len) == 0);
  struct pollfd p = {l, POLLIN, 0};
  CHECK(poll(&p, 1, 1000) == 1 && (p.revents & POLLIN));
  int a = accept4(l, NULL, NULL, SOCK_CLOEXEC);
  CHECK(a >= 0);
  CHECK(fcntl(a, F_GETFD) == FD_CLOEXEC);

  char buf[8];
  CHECK(write(c, "ping", 4) == 4);
  CHECK(read(a, buf, sizeof(buf)) == 4 && memcmp(buf, "ping", 4) == 0);
  CHECK(write(a, "pong", 4) == 4);
  CHECK(read(c, buf, sizeof(buf)) == 4 && memcmp(buf, "pong", 4) == 0);
  close(a);
  close(c);

  /* Closing the listener frees the name. */
  close(l);
  l = socket(AF_UNIX, SOCK_STREAM, 0);
  CHECK(bind(l, (struct sockaddr *)&addr, len) == 0);
  close(l);
}

/* A path name, connected to from a forked child. */
static void test_path_fork(void) {
  const char *path = "/tmp/socktest.sock";
  unlink(path); /* Linux leaves the socket file behind */
  struct sockaddr_un addr = {.sun_family = AF_UNIX};
  strcpy(addr.sun_path, path);

  int l = socket(AF_UNIX, SOCK_STREAM, 0);
  CHECK(bind(l, (struct sockaddr *)&addr, sizeof(addr)) == 0);
  CHECK(listen(l, 1) == 0);

  pid_t pid = fork();
  if (pid == 0) {
    int c = socket(AF_UNIX, SOCK_STREAM, 0);
    if (connect(c, (struct sockaddr *)&addr, sizeof(addr)) != 0)
      _exit(1);
    _exit(write(c, "child", 5) == 5 ? 0 : 2);
  }
  int a = accept(l, NULL, NULL);
  char buf[8];
  CHECK(a >= 0 && read(a, buf, sizeof(buf)) == 5 &&
        memcmp(buf, "child", 5) == 0);
  int status;
  CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) &&
        WEXITSTATUS(status) == 0);
  close(a);
  close(l);
  unlink(path);
}

/* poll wakes for any of its fds, not just the first. */
static void test_poll_many(void) {
  int s1[2], s2[2];
  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, s1) == 0);
  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, s2) == 0);

  pid_t pid = fork();
  if (pid == 0) {
    sleep_ms(200);
    _exit(write(s2[1], "!", 1) == 1 ? 0 : 1);
  }
  struct pollfd p[2] = {{s1[0], POLLIN, 0}, {s2[0], POLLIN, 0}};
  long start = now_ms();
  int n = poll(p, 2, 5000);
  long took = now_ms() - start;
  CHECK(n == 1 && !p[0].revents && (p[1].revents & POLLIN));
  CHECK(took < 2000); /* woken by the write, not the timeout */
  waitpid(pid, NULL, 0);
  for (int i = 0; i < 2; i++) {
    close(s1[i]);
    close(s2[i]);
  }
}

static void test_memfd(void) {
  int fd = memfd_create("socktest", MFD_CLOEXEC);
  CHECK(fd >= 0);
  CHECK(ftruncate(fd, 3 * 4096) == 0);
  struct stat st;
  CHECK(fstat(fd, &st) == 0 && st.st_size == 3 * 4096 && S_ISREG(st.st_mode));

  unsigned *shared = mmap(NULL, 3 * 4096, PROT_READ | PROT_WRITE, MAP_SHARED,
                          fd, 0);
  CHECK(shared != MAP_FAILED);
  if (shared == MAP_FAILED)
    return;
  CHECK(shared[0] == 0 && shared[3 * 1024 - 1] == 0); /* zero-filled */
  shared[0] = 0x1234;

  /* A second mapping is the same memory. */
  unsigned *again = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  CHECK(again != MAP_FAILED && again[0] == 0x1234);
  again[1] = 0x5678;
  CHECK(shared[1] == 0x5678);

  /* A private mapping copies on write. */
  unsigned *priv = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  CHECK(priv != MAP_FAILED && priv[0] == 0x1234);
  priv[0] = 0xdead;
  CHECK(shared[0] == 0x1234 && priv[0] == 0xdead);

  /* Shared with a forked child, both ways. */
  pid_t pid = fork();
  if (pid == 0) {
    int ok = shared[1] == 0x5678;
    shared[2] = 0xc0de;
    _exit(ok ? 0 : 1);
  }
  int status;
  CHECK(waitpid(pid, &status, 0) == pid && WEXITSTATUS(status) == 0);
  CHECK(shared[2] == 0xc0de);

  /* Growing keeps the contents. */
  CHECK(ftruncate(fd, 8 * 4096) == 0);
  unsigned *big = mmap(NULL, 8 * 4096, PROT_READ | PROT_WRITE, MAP_SHARED,
                       fd, 0);
  CHECK(big != MAP_FAILED && big[0] == 0x1234 && big[8 * 1024 - 1] == 0);

  munmap(priv, 4096);
  munmap(again, 4096);
  munmap(big, 8 * 4096);
  munmap(shared, 3 * 4096);
  close(fd);

  /* Shared anonymous memory survives fork as shared. */
  unsigned *anon = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  CHECK(anon != MAP_FAILED);
  pid = fork();
  if (pid == 0) {
    anon[0] = 42;
    _exit(0);
  }
  waitpid(pid, NULL, 0);
  CHECK(anon[0] == 42);
  munmap(anon, 4096);
}

/* The compositor's case: a child makes a buffer and hands it over. */
static void test_pass_memfd(void) {
  int sv[2];
  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

  pid_t pid = fork();
  if (pid == 0) {
    close(sv[0]);
    int fd = memfd_create("window", 0);
    ftruncate(fd, 4096);
    unsigned *px = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    px[0] = 0xff00ff00;
    send_fd(sv[1], "B", 1, fd);
    close(fd); /* the receiver holds it now */
    char ack;
    if (read(sv[1], &ack, 1) != 1) /* parent wrote into the buffer */
      _exit(1);
    _exit(px[1] == 0xabcdef ? 0 : 2);
  }
  close(sv[1]);
  char tag = 0;
  int fd;
  CHECK(recv_fd(sv[0], &tag, 1, &fd) == 1 && tag == 'B' && fd >= 0);
  if (fd >= 0) {
    unsigned *px = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    CHECK(px != MAP_FAILED && px[0] == 0xff00ff00);
    px[1] = 0xabcdef;
    write(sv[0], "k", 1);
    close(fd);
  }
  int status;
  CHECK(waitpid(pid, &status, 0) == pid && WEXITSTATUS(status) == 0);
  close(sv[0]);
}

/* Files arrive once, together with the bytes that were sent with them. */
static void test_rights_boundary(void) {
  int sv[2];
  CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
  int mfd = memfd_create("x", 0);
  CHECK(send_fd(sv[0], "ab", 2, -1) == 2);
  CHECK(send_fd(sv[0], "cd", 2, mfd) == 2);

  char buf[8] = {0};
  size_t got = 0;
  int fds = 0, fd = -1;
  while (got < 4) {
    int f;
    ssize_t n = recv_fd(sv[1], buf + got, sizeof(buf) - got, &f);
    if (n <= 0)
      break;
    /* Linux may merge "ab" into the read that carries the file; ours
     * stops before it. Either way the file comes with "cd". */
    if (f >= 0) {
      fds++;
      fd = f;
      CHECK(got + n == 4);
    }
    got += n;
  }
  CHECK(got == 4 && memcmp(buf, "abcd", 4) == 0 && fds == 1);
  struct stat st;
  CHECK(fd >= 0 && fstat(fd, &st) == 0 && S_ISREG(st.st_mode));
  close(fd);
  close(mfd);
  close(sv[0]);
  close(sv[1]);
}

int main(void) {
  test_socketpair();
  test_listen_connect();
  test_path_fork();
  test_poll_many();
  test_memfd();
  test_pass_memfd();
  test_rights_boundary();
  printf("socktest: %d passed, %d failed\n", passed, failed);
  return failed != 0;
}
