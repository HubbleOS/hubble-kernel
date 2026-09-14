/*
 * Syscall: wait4() - block until a child exits and reap its exit status.
 *
 * task_exit() (scheduler.c) leaves an exited task's struct around in
 * TASK_DEAD state with its exit code intact instead of freeing it - this is
 * the code that collects it: it walks the caller's child list for a
 * TASK_DEAD match, and if none is ready yet (and WNOHANG wasn't requested)
 * blocks on the caller's own child_wait queue until task_exit() wakes it.
 */

#include <hubble/errno.h>

#include <mm/kmalloc.h>
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
long sys_wait4(int pid, int *status, int options, void *rusage) {
  (void)rusage;

  if (pid == 0 || pid < -1)
    return -EINVAL; /* process-group waits: not supported, not silently wrong */

  task_t *parent = get_current_task();

  for (;;) {
    if (!parent->linkage.children)
      return -ECHILD;

    task_t *prev = NULL;
    for (task_t *child = parent->linkage.children; child;
        prev = child, child = child->linkage.sibling) {
      if (pid > 0 && (int)child->id.pid != pid)
        continue;
      if (child->linkage.state != TASK_DEAD)
        continue;

      /* Found an exited child: unlink and reap it. */
      if (prev)
        prev->linkage.sibling = child->linkage.sibling;
      else
        parent->linkage.children = child->linkage.sibling;

      if (status)
        *status = (child->linkage.exit_code & 0xff) << 8;

      uint32_t reaped_pid = child->id.pid;
      kfree(child->child_wait);
      kfree(child);
      return (long)reaped_pid;
    }

    if (options & WNOHANG)
      return 0;

    waitqueue_sleep(parent->child_wait);
  }
}
