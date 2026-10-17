/*
 * ptytest - checks pseudo-terminals, sessions and process groups.
 *
 * Every check states what Linux does, so the same binary is the
 * reference on a Linux host and the test on HubbleOS:
 *
 *   ptytest      prints each failed check, then "N passed, M failed"
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
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

/* Read from @p fd until @p want bytes arrived or 2 s passed. */
static ssize_t read_n(int fd, char *buf, size_t want) {
  size_t got = 0;
  while (got < want) {
    struct pollfd p = {fd, POLLIN, 0};
    if (poll(&p, 1, 2000) <= 0)
      break;
    ssize_t n = read(fd, buf + got, want - got);
    if (n <= 0)
      return got ? (ssize_t)got : n;
    got += n;
  }
  buf[got] = '\0';
  return (ssize_t)got;
}

/* A new pty: master in *m, its slave's path in name. */
static int open_pty(int *m, char *name, size_t len) {
  *m = posix_openpt(O_RDWR | O_NOCTTY);
  if (*m < 0 || grantpt(*m) < 0 || unlockpt(*m) < 0)
    return -1;
  return ptsname_r(*m, name, len);
}

static void test_basic(void) {
  int m;
  char name[64];
  CHECK(open_pty(&m, name, sizeof(name)) == 0);
  CHECK(strncmp(name, "/dev/pts/", 9) == 0);
  int s = open(name, O_RDWR | O_NOCTTY);
  CHECK(s >= 0);
  CHECK(isatty(s) && isatty(m));

  /* Typing a line: the slave reads it, the master sees the echo. */
  char buf[64];
  CHECK(write(m, "hi\r", 3) == 3);
  CHECK(read_n(s, buf, 3) == 3 && strcmp(buf, "hi\n") == 0);
  CHECK(read_n(m, buf, 4) == 4 && strcmp(buf, "hi\r\n") == 0);

  /* Output: "\n" reaches the screen as "\r\n" (ONLCR). */
  CHECK(write(s, "out\n", 4) == 4);
  CHECK(read_n(m, buf, 5) == 5 && strcmp(buf, "out\r\n") == 0);

  /* Raw mode: each byte at once, no echo. */
  struct termios t;
  CHECK(tcgetattr(s, &t) == 0);
  cfmakeraw(&t);
  CHECK(tcsetattr(s, TCSANOW, &t) == 0);
  CHECK(write(m, "x", 1) == 1);
  CHECK(read_n(s, buf, 1) == 1 && buf[0] == 'x');
  struct pollfd p = {m, POLLIN, 0};
  CHECK(poll(&p, 1, 100) == 0); /* nothing echoed */

  /* The window size the master sets is the slave's. */
  struct winsize ws = {.ws_row = 33, .ws_col = 111};
  CHECK(ioctl(m, TIOCSWINSZ, &ws) == 0);
  memset(&ws, 0, sizeof(ws));
  CHECK(ioctl(s, TIOCGWINSZ, &ws) == 0 && ws.ws_row == 33 &&
        ws.ws_col == 111);

  /* Closing the master hangs up the slave. */
  close(m);
  CHECK(read(s, buf, sizeof(buf)) <= 0);
  close(s);
}

/* What forkpty() does: the child gets its own session, with the slave
 * as its controlling terminal; when it exits, the master reads EIO. */
static void test_session(void) {
  int m;
  char name[64];
  CHECK(open_pty(&m, name, sizeof(name)) == 0);

  pid_t pid = fork();
  if (pid == 0) {
    close(m);
    if (setsid() != getpid())
      _exit(1);
    int s = open(name, O_RDWR); /* becomes the controlling terminal */
    if (s < 0)
      _exit(2);
    pid_t sid = 0;
    if (ioctl(s, TIOCGSID, &sid) < 0 || sid != getpid())
      _exit(3);
    if (tcgetpgrp(s) != getpid())
      _exit(4);
    if (getsid(0) != getpid() || getpgrp() != getpid())
      _exit(5);
    if (setsid() != -1 || errno != EPERM) /* already a leader */
      _exit(6);
    dprintf(s, "child ok\n");
    _exit(0);
  }

  char buf[64];
  CHECK(read_n(m, buf, 10) == 10 && strcmp(buf, "child ok\r\n") == 0);
  int status;
  CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status));
  CHECK(WEXITSTATUS(status) == 0);
  if (WEXITSTATUS(status))
    printf("  child exit %d\n", WEXITSTATUS(status));
  /* The shell is gone. */
  CHECK(read(m, buf, sizeof(buf)) == -1 && errno == EIO);
  close(m);
}

/* forkpty(): what tmux does for each pane. The child is in a new
 * session with the slave as its controlling terminal on fds 0-2. */
static void test_forkpty(void) {
  int m;
  struct winsize ws = {.ws_row = 20, .ws_col = 70};
  pid_t pid = forkpty(&m, NULL, NULL, &ws);
  CHECK(pid >= 0);
  if (pid < 0)
    return;
  if (pid == 0) {
    struct winsize got;
    int ok = getsid(0) == getpid() && isatty(0) && isatty(1) &&
             tcgetpgrp(0) == getpid() && ioctl(0, TIOCGWINSZ, &got) == 0 &&
             got.ws_col == 70;
    printf(ok ? "pane ok\n" : "pane bad\n");
    _exit(0);
  }
  char buf[64];
  CHECK(read_n(m, buf, 9) == 9 && strcmp(buf, "pane ok\r\n") == 0);
  waitpid(pid, NULL, 0);
  close(m);
}

static void test_pgrp(void) {
  pid_t pid = fork();
  if (pid == 0) {
    usleep(300000); /* long enough to be moved */
    _exit(0);
  }
  CHECK(setpgid(pid, pid) == 0);
  CHECK(getpgid(pid) == pid);
  CHECK(getpgid(0) == getpgrp());
  CHECK(getpgid(999999) == -1 && errno == ESRCH);
  waitpid(pid, NULL, 0);
}

int main(void) {
  test_basic();
  test_session();
  test_forkpty();
  test_pgrp();
  printf("ptytest: %d passed, %d failed\n", passed, failed);
  return failed != 0;
}
