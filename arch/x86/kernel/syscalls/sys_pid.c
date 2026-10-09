#include <hubble/syscalls.h>
#include <smp/scheduler.h>

long sys_getpid(void) {
  task_t *task = get_current_task();
  return task ? task->id.pid : 0;
}

long sys_getppid(void) {
  task_t *task = get_current_task();
  return task && task->linkage.parent ? task->linkage.parent->id.pid : 0;
}

/* Every task is its own process (no shared-address-space threads), so
 * the thread id is the process id, as for a Linux main thread. */
long sys_gettid(void) { return sys_getpid(); }
