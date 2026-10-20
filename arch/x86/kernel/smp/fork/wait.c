/*
 * Syscall: wait4() - block until a child exits and reap its exit status.
 *
 * task_exit() (scheduler.c) leaves an exited task's struct around in
 * TASK_DEAD state with its exit code intact instead of freeing it - this is
 * the code that collects it (task_reap_child), and if none is ready yet
 * (and WNOHANG wasn't requested) blocks on the caller's own child_wait
 * queue until task_exit() wakes it.
 */

#include <hpet/hpet.h>
#include <hubble/errno.h>
#include <hubble/string.h>
#include <hubble/syscalls.h>

#include <smp/scheduler.h>
#include <smp/task.h>
#include <smp/waitqueue.h>

#define WNOHANG 1
#define SIGCHLD 17
#define CLD_EXITED 1
#define SIGINFO_SIZE 128

/**
 * @brief Handle the wait4() syscall
 *
 * @param pid     -1 to wait for any child, or a specific child pid;
 *                0 / negative-other-than-1 (process-group waits) is not
 *                supported by this kernel
 * @param status  Where to store the child's exit status (WIFEXITED/
 *                WEXITSTATUS shape), or NULL to discard it
 * @param options WNOHANG to return 0 instead of blocking when no child has
 *                exited yet
 * @param rusage  Unused - resource usage accounting isn't implemented
 * @return Reaped child's pid, 0 (WNOHANG, nothing ready), or a negative
 *         errno
 */
struct wait_ctx {
  task_t *parent;
  int pid;
};

/* A wait4 can return: a matching child exited, or there is none. */
static bool child_ready(void *arg) {
  struct wait_ctx *ctx = arg;
  return task_child_waitable(ctx->parent, ctx->pid);
}

long sys_wait4(int pid, int *status, int options, void *rusage) {
  (void)rusage;

  if (pid == 0 || pid < -1)
    return -EINVAL; /* process-group waits: not supported, not silently wrong */

  task_t *parent = get_current_task();
  struct wait_ctx ctx = {parent, pid};

  for (;;) {
    int code;
    long ret = task_reap_child(parent, pid, &code);
    if (ret > 0 && status)
      *status = (code & 0xff) << 8;
    if (ret != 0 || (options & WNOHANG))
      return ret;
    /* Race-free: an exit between the check above and the sleep still
     * wakes us. */
    waitqueue_wait_event(parent->child_wait, child_ready, &ctx, 0);
  }
}

static bool child_exited(void *arg) { return task_child_exited(arg); }
static bool never(void *arg) {
  (void)arg;
  return false;
}

/**
 * @brief Handle rt_sigtimedwait() for the one signal this kernel raises
 *
 * There is no signal delivery yet. SIGCHLD is the exception that can be
 * waited for: it counts as pending while the caller has an exited child
 * not yet reaped, which is when Linux would have raised it. Unlike Linux,
 * taking it does not clear it - reaping the child does. Any other signal
 * never comes, so waiting for one just waits out the timeout. busybox
 * init sleeps here until a child dies.
 *
 * @return SIGCHLD, -EAGAIN on timeout, or a negative errno
 */
long sys_rt_sigtimedwait(const uint64_t *set, void *info,
                         const struct kernel_timespec *timeout,
                         size_t sigsetsize) {
  if (!set || sigsetsize != sizeof(*set))
    return -EINVAL;

  uint64_t deadline = 0; /* none: wait for good */
  if (timeout) {
    if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
        timeout->tv_nsec >= (int64_t)NS_PER_SEC)
      return -EINVAL;
    deadline = hpet_get_time_ns() + (uint64_t)timeout->tv_sec * NS_PER_SEC +
               timeout->tv_nsec;
  }

  task_t *self = get_current_task();
  bool want = *set & (1ull << (SIGCHLD - 1));
  if (!waitqueue_wait_event(self->child_wait, want ? child_exited : never, self,
                            deadline))
    return -EAGAIN;

  if (info) {
    memset(info, 0, SIGINFO_SIZE);
    ((int32_t *)info)[0] = SIGCHLD;    /* si_signo */
    ((int32_t *)info)[2] = CLD_EXITED; /* si_code */
  }
  return SIGCHLD;
}
