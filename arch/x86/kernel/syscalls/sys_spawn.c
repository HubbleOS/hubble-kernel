/*
 * Syscalls: spawn new tasks.
 *
 * Implements sys_spawn (create a task from a raw entry point) and
 * sys_spawn_file (load and execute a file as a new task).
 */

#include <hubble/errno.h>
#include <hubble/syscalls.h>

#include <smp/scheduler.h>
#include <smp/task.h>
#include <user/exec.h>

#include "syscall_entry.h"
#include <asm.h>

/**
 * @brief Spawn a new task from a raw entry point.
 *
 * Creates a new task that begins execution at @p entry_point with the
 * given @p priority and adds it to the scheduler.
 *
 * @param entry_point Address at which the new task should start executing.
 * @param arg         Argument passed to the new task (currently unused).
 * @param priority    Scheduling priority for the new task.
 *
 * @return PID of the new task on success, or -1 on error.
 */
long sys_spawn(void *entry_point, void *arg, uint32_t priority) {
  (void)arg;
  if (!entry_point)
    return -EINVAL;

  task_t *task = task_create((void *)entry_point, priority, 1);
  if (!task)
    return -EPERM;

  task_map_user_stack(task, (uint64_t *)get_cr3());

  scheduler_add_task(task);
  return (long)task->id.pid;
}

/**
 * @brief Spawn a new task from a file.
 *
 * Loads the executable at @p path into a new task's address space,
 * sets its priority, and adds it to the scheduler.
 *
 * @param path     Path to the executable file.
 * @param arg      Argument passed to the new task (currently unused).
 * @param priority Scheduling priority for the new task.
 *
 * @return PID of the new task on success, or -1 on error.
 */
long sys_spawn_file(const char *path, void *arg, uint32_t priority) {
  (void)arg;

  task_t *task = exec(path);
  task->sched.priority = priority;
  task->sched.time_slice_max = 5 + priority;
  task->sched.time_slice = task->sched.time_slice_max;
  scheduler_add_task(task);
  return (long)task->id.pid;
}
