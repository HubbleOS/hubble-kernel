/*
 * Syscall: wait4() - block until a child exits (or stops) and collect its
 * status.
 *
 * task_exit() (scheduler.c) leaves an exited task's struct around in
 * TASK_DEAD state with its wait status intact instead of freeing it - this
 * is the code that collects it (task_wait_child), and if none is ready yet
 * (and WNOHANG wasn't requested) blocks on the caller's own child_wait
 * queue until task_exit() wakes it.
 */

#include <hubble/errno.h>
#include <hubble/signal.h>
#include <hubble/string.h>

#include <smp/scheduler.h>
#include <smp/task.h>
#include <smp/waitqueue.h>

/**
 * @brief Handle the wait4() syscall
 *
 * @param pid     A child's pid, -1 for any child, 0 for any in the
 *                caller's process group, < -1 for any in group -pid
 * @param status  Where to store the wait status (WIFEXITED / WIFSIGNALED
 *                / WIFSTOPPED / WIFCONTINUED shape), or NULL to discard it
 * @param options WNOHANG to return 0 instead of blocking; WUNTRACED and
 *                WCONTINUED to also report children that stopped or
 *                continued
 * @param rusage  Zeroed if given: resource usage isn't accounted
 * @return The child's pid, 0 (WNOHANG, nothing ready), or a negative
 *         errno
 */
struct wait_ctx {
  task_t *parent;
  int pid;
  int options;
};

/* A wait4 can return: a matching child has news, or there is none. */
static bool child_ready(void *arg) {
  struct wait_ctx *ctx = arg;
  return task_child_waitable(ctx->parent, ctx->pid, ctx->options);
}

/* struct rusage: two timevals and 14 longs. */
#define RUSAGE_SIZE 144

long sys_wait4(int pid, int *status, int options, void *rusage) {
  /* Left as it was, callers read garbage (busybox time: "13h user"). */
  if (rusage)
    memset(rusage, 0, RUSAGE_SIZE);

  task_t *parent = get_current_task();
  struct wait_ctx ctx = {parent, pid, options};

  for (;;) {
    int code;
    long ret = task_wait_child(parent, pid, options, &code);
    if (ret > 0 && status)
      *status = code;
    if (ret != 0 || (options & WNOHANG))
      return ret;
    /* Race-free: news between the check above and the sleep still
     * wakes us. */
    long err = waitqueue_wait_event(parent->child_wait, child_ready, &ctx, 0);
    if (err)
      return err;
  }
}
