#include <mm/kmalloc.h>
#include <smp/scheduler.h>
#include <smp/smp.h>
#include <smp/task.h>

#include <hubble/string.h>

int fork(task_t *parent_task, int userspace) {
  task_t *child_task = _task_create_with_arg(NULL, NULL, 0, userspace);

  child_task->sched = parent_task->sched;
  child_task->exec = parent_task->exec;
  return 0;
}
