/*
 * nettest: TCP over 127.0.0.1 waited on with select, pselect, ppoll and
 * epoll, their timeouts, and a signal ending (or, masked, not ending) a
 * wait. Prints ok/FAIL per check; exits non-zero if any failed.
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("ok   " __VA_ARGS__), printf("\n"); else { printf("FAIL " __VA_ARGS__); printf(" (errno %d %s)\n", errno, strerror(errno)); fails++; } } while (0)

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static void on_alrm(int s) { (void)s; }

static int listener(int *port) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in a = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
  bind(s, (void *)&a, sizeof a);
  listen(s, 4);
  socklen_t l = sizeof a;
  getsockname(s, (void *)&a, &l);
  *port = ntohs(a.sin_port);
  return s;
}
static int dial(int port) {
  int c = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
  return connect(c, (void *)&a, sizeof a) ? -1 : c;
}

int main(void) {
  int port, ls = listener(&port);
  CHECK(port > 0, "listen on 127.0.0.1:%d", port);

  /* select: timeout on an idle listener, then readable after connect */
  fd_set r; FD_ZERO(&r); FD_SET(ls, &r);
  struct timeval tv = {0, 200000};
  double t0 = now();
  int n = select(ls + 1, &r, NULL, NULL, &tv);
  double dt = now() - t0;
  CHECK(n == 0 && dt > 0.15 && dt < 1.0, "select times out (%.3fs)", dt);
  int c = dial(port);
  CHECK(c >= 0, "connect");
  FD_ZERO(&r); FD_SET(ls, &r); tv = (struct timeval){2, 0};
  n = select(ls + 1, &r, NULL, NULL, &tv);
  CHECK(n == 1 && FD_ISSET(ls, &r), "select: listener readable");
  int a = accept(ls, NULL, NULL);
  CHECK(a >= 0, "accept");

  /* select: write side ready, read side not, then data */
  fd_set w; FD_ZERO(&r); FD_ZERO(&w); FD_SET(a, &r); FD_SET(c, &w);
  tv = (struct timeval){0, 0};
  n = select((a > c ? a : c) + 1, &r, &w, NULL, &tv);
  CHECK(n == 1 && FD_ISSET(c, &w) && !FD_ISSET(a, &r), "select: writable, not readable");
  write(c, "ping", 4);
  FD_ZERO(&r); FD_SET(a, &r); tv = (struct timeval){2, 0};
  n = select(a + 1, &r, NULL, NULL, &tv);
  char buf[16] = {0};
  CHECK(n == 1 && read(a, buf, sizeof buf) == 4 && !memcmp(buf, "ping", 4), "select: data arrives");
  FD_ZERO(&r); FD_SET(60, &r);
  CHECK(select(61, &r, NULL, NULL, &(struct timeval){0, 0}) < 0 && errno == EBADF, "select: closed fd -> EBADF");

  /* pselect */
  FD_ZERO(&r); FD_SET(a, &r);
  write(c, "x", 1);
  n = pselect(a + 1, &r, NULL, NULL, &(struct timespec){2, 0}, NULL);
  CHECK(n == 1 && read(a, buf, 1) == 1, "pselect: readable");

  /* ppoll with a timeout */
  struct pollfd p = {.fd = a, .events = POLLIN};
  t0 = now();
  n = ppoll(&p, 1, &(struct timespec){0, 100000000}, NULL);
  dt = now() - t0;
  CHECK(n == 0 && dt > 0.05, "ppoll times out (%.3fs)", dt);

  /* a signal ends the wait with EINTR; the mask ppoll was given blocks it */
  struct sigaction sa = {.sa_handler = on_alrm};
  sigaction(SIGALRM, &sa, NULL);
  struct itimerval it = {.it_value = {0, 100000}};
  setitimer(ITIMER_REAL, &it, NULL);
  n = poll(&p, 1, 3000);
  CHECK(n < 0 && errno == EINTR, "poll: SIGALRM -> EINTR");
  sigset_t block; sigemptyset(&block); sigaddset(&block, SIGALRM);
  setitimer(ITIMER_REAL, &it, NULL);
  t0 = now();
  n = ppoll(&p, 1, &(struct timespec){0, 400000000}, &block);
  dt = now() - t0;
  CHECK(n == 0 && dt > 0.3, "ppoll: masked SIGALRM does not interrupt (%.3fs)", dt);
  /* the pending SIGALRM is delivered once the old mask is back */
  sigset_t pend; sigpending(&pend);
  CHECK(!sigismember(&pend, SIGALRM), "ppoll: old mask restored, SIGALRM delivered");

  /* epoll */
  int ep = epoll_create1(EPOLL_CLOEXEC);
  CHECK(ep >= 0, "epoll_create1");
  struct epoll_event ev = {.events = EPOLLIN, .data.u64 = 0x1234};
  CHECK(epoll_ctl(ep, EPOLL_CTL_ADD, a, &ev) == 0, "epoll_ctl ADD");
  CHECK(epoll_ctl(ep, EPOLL_CTL_ADD, a, &ev) < 0 && errno == EEXIST, "epoll_ctl ADD again -> EEXIST");
  struct epoll_event out[4];
  t0 = now();
  n = epoll_wait(ep, out, 4, 150);
  dt = now() - t0;
  CHECK(n == 0 && dt > 0.1, "epoll_wait times out (%.3fs)", dt);
  write(c, "hey", 3);
  n = epoll_wait(ep, out, 4, 2000);
  CHECK(n == 1 && out[0].data.u64 == 0x1234 && (out[0].events & EPOLLIN), "epoll_wait: EPOLLIN with data");
  read(a, buf, sizeof buf);
  ev = (struct epoll_event){.events = EPOLLOUT | EPOLLONESHOT, .data.fd = c};
  epoll_ctl(ep, EPOLL_CTL_ADD, c, &ev);
  n = epoll_wait(ep, out, 4, 0);
  CHECK(n == 1 && out[0].data.fd == c && (out[0].events & EPOLLOUT), "epoll: oneshot EPOLLOUT");
  n = epoll_wait(ep, out, 4, 0);
  CHECK(n == 0, "epoll: oneshot reported once");
  struct pollfd pe = {.fd = ep, .events = POLLIN};
  CHECK(poll(&pe, 1, 0) == 0, "poll on epoll fd: not ready");
  write(c, "z", 1);
  CHECK(poll(&pe, 1, 1000) == 1, "poll on epoll fd: ready");
  close(c);
  n = epoll_wait(ep, out, 4, 1000);
  CHECK(n == 1 && (out[0].events & (EPOLLIN | EPOLLHUP)), "epoll: peer close reported (events %#x)", n == 1 ? out[0].events : 0);
  CHECK(epoll_ctl(ep, EPOLL_CTL_DEL, a, NULL) == 0, "epoll_ctl DEL");
  int fd0 = open("/etc/hosts", O_RDONLY);
  CHECK(epoll_ctl(ep, EPOLL_CTL_ADD, fd0, &ev) < 0 && errno == EPERM, "epoll: regular file -> EPERM");

  printf("%s: %d failed\n", fails ? "NETTEST FAILED" : "NETTEST PASSED", fails);
  return fails != 0;
}
