/*
 * Syscall: wait4() - block until a child exits and reap its exit status.
 *
 * task_exit() (scheduler.c) leaves an exited task's struct around in
 * TASK_DEAD state with its exit code intact instead of freeing it - this is
 * the code that collects it (task_reap_child), and if none is ready yet
 * (and WNOHANG wasn't requested) blocks on the caller's own child_wait
 * queue until task_exit() wakes it.
 */

#include <hubble/errno.h>

#include <smp/scheduler.h>
#include <smp/task.h>
#include <smp/waitqueue.h>

#define WNOHANG 1

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
