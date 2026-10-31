#pragma once

/**
 * @brief System call dispatcher and individual handler declarations.
 */

#include <stddef.h>
#include <stdint.h>

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5, uint64_t a6);

long sys_read(int fd, char *buf, size_t count);
long sys_mmap(uint64_t addr, size_t length, int prot, int flags, int fd,
              uint64_t offset);
long sys_write(int fd, const char *buf, size_t count);
long sys_open(const char *path, int flags, int mode);
long sys_lseek(int fd, uint64_t offset, int whence);
long sys_sendfile(int out_fd, int in_fd, int64_t *offset, size_t count);
long sys_close(int fd);
long sys_dup(int oldfd);
long sys_dup2(int oldfd, int newfd);
long sys_dup3(int oldfd, int newfd, int flags);
long sys_pipe(int fds[2]);
long sys_pipe2(int fds[2], int flags);
long sys_spawn(void *entry_point, void *arg, uint32_t priority);
long sys_spawn_file(const char *path, void *arg, uint32_t priority);
long sys_module_load(const char *path);
long sys_module_unload(const char *name);
uint64_t sys_brk(uint64_t new_addr);
long sys_stat(const char *path, struct stat *st);
long sys_lstat(const char *path, struct stat *st);
long sys_fstat(int fd, struct stat *st);
long sys_writev(int fd, struct iovec *iov, int iovcnt);
long sys_readv(int fd, struct iovec *iov, int iovcnt);
long arch_prctl(long a1, long a2, long a3, long a4, long a5, long a6);
long sys_munmap(uint64_t addr, size_t length);
long sys_mprotect(uint64_t addr, size_t len, int prot);
long sys_pread64(int fd, char *buf, size_t count, int64_t offset);
long sys_pwrite64(int fd, const char *buf, size_t count, int64_t offset);
long sys_newfstatat(int dirfd, const char *path, struct stat *st, int flags);
long sys_readlinkat(int dirfd, const char *path, char *buf, size_t size);
long sys_set_robust_list(void *head, size_t len);
long sys_rseq(void *rseq, uint32_t len, int flags, uint32_t sig);
long sys_getrandom(void *buf, size_t len, unsigned int flags);

typedef struct registers registers_t;
long sys_fork(registers_t *regs);
long sys_clone(registers_t *regs);
long sys_execve(registers_t *regs);
long sys_wait4(int pid, int *status, int options, void *rusage);

/* Signals. The rt_sig* ones and pause are Rust (rust/src/signal.rs). */
struct kernel_timespec;
struct registers;
long sys_rt_sigaction(int sig, const void *act, void *oldact,
                      size_t sigsetsize);
long sys_rt_sigprocmask(int how, const void *set, void *oldset,
                        size_t sigsetsize);
long sys_rt_sigpending(uint64_t *set, size_t sigsetsize);
long sys_rt_sigsuspend(const uint64_t *set, size_t sigsetsize);
long sys_rt_sigtimedwait(const uint64_t *set, void *info,
                         const struct kernel_timespec *timeout,
                         size_t sigsetsize);
long sys_rt_sigreturn(struct registers *regs);
long sys_pause(void);
long sys_kill(int pid, int sig);
long sys_tkill(int tid, int sig);
long sys_tgkill(int tgid, int tid, int sig);
long sys_getuid(void);
long sys_geteuid(void);
long sys_getgid(void);
long sys_getegid(void);
long sys_readlink(const char *path, char *buf, size_t size);
long sys_chmod(const char *path, unsigned int mode);
long sys_fchmod(int fd, unsigned int mode);
long sys_umask(unsigned int mask);
long sys_madvise(uint64_t addr, size_t length, int advice);
long sys_flock(int fd, int op);
struct utsname;
long sys_uname(struct utsname *buf);
long sys_setsid(void);
long sys_setpgid(int pid, int pgid);
long sys_getpgid(int pid);
long sys_getpgrp(void);
long sys_getsid(int pid);

struct sockaddr_un;
struct msghdr;
long sys_socket(int domain, int type, int protocol);
long sys_socketpair(int domain, int type, int protocol, int sv[2]);
long sys_bind(int fd, const struct sockaddr_un *addr, uint32_t len);
long sys_listen(int fd, int backlog);
long sys_accept(int fd, struct sockaddr_un *addr, uint32_t *len);
long sys_accept4(int fd, struct sockaddr_un *addr, uint32_t *len, int flags);
long sys_connect(int fd, const struct sockaddr_un *addr, uint32_t len);
long sys_shutdown(int fd, int how);
long sys_sendto(int fd, const void *buf, size_t len, int flags,
                const struct sockaddr_un *dest, uint32_t dest_len);
long sys_recvfrom(int fd, void *buf, size_t len, int flags,
                  struct sockaddr_un *src, uint32_t *src_len);
long sys_sendmsg(int fd, const struct msghdr *msg, int flags);
long sys_recvmsg(int fd, struct msghdr *msg, int flags);
long sys_memfd_create(const char *name, unsigned int flags);
long sys_getsockopt(int fd, int level, int name, void *val, uint32_t *len);
long sys_setsockopt(int fd, int level, int name, const void *val,
                    uint32_t len);
long sys_prctl(int option, unsigned long arg2, unsigned long arg3,
               unsigned long arg4, unsigned long arg5);
struct rlimit64;
long sys_prlimit64(int pid, int resource, const struct rlimit64 *new_limit,
                   struct rlimit64 *old_limit);
long sys_getrlimit(int resource, struct rlimit64 *limit);
long sys_setrlimit(int resource, const struct rlimit64 *limit);
long sys_getpid(void);
long sys_getppid(void);
long sys_gettid(void);
long sys_geteuid(void);
long sys_getcwd(char *buf, size_t size);
long sys_chdir(const char *path);
long sys_access(const char *path, int mode);
long sys_openat(int dirfd, const char *path, int flags, int mode);
long sys_ftruncate(int fd, long length);
long sys_truncate(const char *path, long length);
long sys_unlink(const char *path);
long sys_rmdir(const char *path);
long sys_mkdir(const char *path, int mode);
long sys_rename(const char *from, const char *to);
long sys_fsync(int fd);
long sys_getdents64(int fd, void *dirp, size_t count);
long sys_ioctl(int fd, unsigned long request, void *arg);
long sys_fcntl(int fd, int cmd, long arg);
struct pollfd;
long sys_poll(struct pollfd *fds, unsigned long nfds, int timeout_ms);
#define NS_PER_SEC 1000000000ULL
struct kernel_timespec {
  int64_t tv_sec;
  int64_t tv_nsec;
};
long sys_restart_syscall(void);
long sys_nanosleep(const struct kernel_timespec *req,
                   struct kernel_timespec *rem);
long sys_clock_gettime(int clock_id, void *ts);
long sys_gettimeofday(void *tv, void *tz);
long sys_time(int64_t *tloc);
long sys_futex(uint32_t *uaddr, int op, uint32_t val,
               const struct kernel_timespec *timeout, uint32_t *uaddr2,
               uint32_t val3);
long sys_clone3(void *args, size_t size);
long exit_stub(long a1);
