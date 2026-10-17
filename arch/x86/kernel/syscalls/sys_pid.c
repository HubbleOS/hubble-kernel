#include <hubble/errno.h>
#include <hubble/string.h>
#include <hubble/syscalls.h>
#include <mm/kmalloc.h>
#include <smp/scheduler.h>

long sys_getpid(void) {
  task_t *task = get_current_task();
  return task ? task->id.pid : 0;
}

long sys_getppid(void) {
  task_t *task = get_current_task();
  return task ? task_ppid(task) : 0;
}

/* Every task is its own process (no shared-address-space threads), so
 * the thread id is the process id, as for a Linux main thread. */
long sys_gettid(void) { return sys_getpid(); }

/* -- Sessions and process groups --------------------------
 * A session is a login (or a terminal window, or a tmux pane): its
 * leader is the process that called setsid(), and it can have one
 * controlling terminal (TIOCSCTTY, drivers/tty). Process groups split a
 * session into jobs. */

long sys_setsid(void) {
  task_t *task = get_current_task();
  /* A group leader can't start a session: its group would span two. */
  if (task->pgid == task->id.pid)
    return -EPERM;
  task->sid = task->pgid = task->id.pid;
  return task->sid;
}

long sys_setpgid(int pid, int pgid) {
  if (pid < 0 || pgid < 0)
    return -EINVAL;
  return task_setpgid(get_current_task(), (uint32_t)pid, (uint32_t)pgid);
}

/* @p field of task @p pid (0: the caller), or -ESRCH. */
static long task_field(int pid, bool want_sid) {
  task_t *task = get_current_task();
  if (pid < 0)
    return -ESRCH;
  if (pid == 0 || (uint32_t)pid == task->id.pid)
    return want_sid ? task->sid : task->pgid;
  task_t *copy = kmalloc(sizeof(*copy), GFP_KERNEL); /* 1.8 KB */
  if (!copy)
    return -ENOMEM;
  uint32_t ppid;
  long ret = -ESRCH;
  if (task_get((uint32_t)pid, copy, &ppid))
    ret = want_sid ? copy->sid : copy->pgid;
  kfree(copy);
  return ret;
}

long sys_getpgid(int pid) { return task_field(pid, false); }
long sys_getpgrp(void) { return task_field(0, false); }
long sys_getsid(int pid) { return task_field(pid, true); }

/* -- prctl ------------------------------------------------ */

#define PR_SET_NAME 15
#define PR_GET_NAME 16
#define TASK_COMM_LEN 16

/* The name ps and /proc show; Linux keeps 15 characters of it. */
long sys_prctl(int option, unsigned long arg2, unsigned long arg3,
               unsigned long arg4, unsigned long arg5) {
  (void)arg3, (void)arg4, (void)arg5;
  task_t *task = get_current_task();
  char *name = (char *)arg2;
  if (!name)
    return -EFAULT;
  switch (option) {
  case PR_SET_NAME:
    memset(task->id.name, 0, sizeof(task->id.name));
    for (int i = 0; i < TASK_COMM_LEN - 1 && name[i]; i++)
      task->id.name[i] = name[i];
    return 0;
  case PR_GET_NAME:
    memcpy(name, task->id.name, TASK_COMM_LEN - 1);
    name[TASK_COMM_LEN - 1] = '\0';
    return 0;
  default:
    return -EINVAL;
  }
}

/* -- Resource limits ---------------------------------------
 * Nothing is limited except the descriptor table's fixed size. */

#define RLIMIT_NOFILE 7
#define RLIM_NLIMITS 16
#define RLIM_INFINITY (~0ULL)

struct rlimit64 {
  uint64_t cur;
  uint64_t max;
};

long sys_prlimit64(int pid, int resource, const struct rlimit64 *new_limit,
                   struct rlimit64 *old_limit) {
  if (pid != 0 && (uint32_t)pid != get_current_task()->id.pid)
    return -EPERM; /* other processes' limits: not supported */
  if (resource < 0 || resource >= RLIM_NLIMITS)
    return -EINVAL;
  uint64_t max = resource == RLIMIT_NOFILE ? MAX_FDS : RLIM_INFINITY;
  if (old_limit)
    *old_limit = (struct rlimit64){max, max};
  if (new_limit && (new_limit->cur > new_limit->max || new_limit->max > max))
    return -EPERM; /* can't be raised past what there is */
  return 0;
}

long sys_getrlimit(int resource, struct rlimit64 *limit) {
  return sys_prlimit64(0, resource, NULL, limit);
}

long sys_setrlimit(int resource, const struct rlimit64 *limit) {
  return sys_prlimit64(0, resource, limit, NULL);
}
