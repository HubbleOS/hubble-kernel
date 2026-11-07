/*
 * Signals: the C side.
 *
 * The signal core is Rust (rust/src/signal.rs): per-task state, what a
 * signal does, the frame a handler runs on and most signal syscalls. This
 * file is what needs the scheduler: finding the tasks a kill() means,
 * waking them, stopping and killing tasks, and the hooks the entry code
 * calls on its way back to user space.
 */

#include <hpet/hpet.h>
#include <hubble/errno.h>
#include <hubble/printk.h>
#include <hubble/signal.h>
#include <hubble/syscalls.h>

#include <mm/vmm.h>
#include <smp/scheduler.h>
#include <smp/task.h>

#include <interrupt/interrupt.h>

#define USER_END 0x0000800000000000ULL

/* -- Helpers for the Rust side ------------------------------------------ */

task_signals_t *signal_current(void) {
  return &get_current_task()->signals;
}

long signal_wait(bool (*cond)(void *), void *arg, uint64_t deadline_ns) {
  for (;;) {
    if (cond(arg))
      return 0;
    if (deadline_ns && hpet_get_time_ns() >= deadline_ns)
      return -EAGAIN;
    /* Blocked before the last check: a signal sent after it wakes us. */
    task_prepare_wait();
    if (cond(arg)) {
      task_cancel_wait();
      return 0;
    }
    task_wait(deadline_ns);
  }
}

bool user_mapped(uint64_t addr, uint64_t len) {
  if (!len || addr >= USER_END || len > USER_END - addr)
    return false;
  uint64_t *pml4 = get_current_task()->mm.page_table;
  for (uint64_t page = addr & ~(uint64_t)(VMM_PAGE_SIZE - 1); page < addr + len;
       page += VMM_PAGE_SIZE)
    if (!vmm_get_phys_from(pml4, page))
      return false;
  return true;
}

/* -- Sending ------------------------------------------------------------ */

void signal_send(task_t *task, int sig, const struct sig_info *info) {
  if (!task || !task->abi)
    return; /* kernel threads take no signals */

  sig_queue(&task->signals, sig, info);
  /* A SIGCONT that ended a stop is news for the parent's wait4. */
  if (sig == SIGCONT && sig_report(&task->signals, WCONTINUED, false))
    task_notify_parent(task, CLD_CONTINUED, SIGCONT);

  /* Wherever it sleeps, it wakes and looks: a blocking call returns
   * -ERESTARTSYS, a stopped task checks whether it may go on. */
  task_wake(task);
  scheduler_kick(task);
  need_resched = true;
}

/* Which tasks a kill(pid) means, and who sends. */
struct kill_target {
  int pid;
  int sig;
  task_t *sender;
  struct sig_info info;
  int found;
};

static void kill_one(task_t *task, void *arg) {
  struct kill_target *k = arg;
  if (!task->abi)
    return;

  bool match;
  if (k->pid > 0)
    match = (int)task->id.pid == k->pid;
  else if (k->pid == 0)
    match = task->pgid == k->sender->pgid;
  else if (k->pid == -1) /* everyone but init and the caller */
    match = task->id.pid != 1 && task != k->sender;
  else
    match = (int)task->pgid == -k->pid;
  if (!match)
    return;

  k->found++;
  if (k->sig) /* signal 0 only checks the target exists */
    signal_send(task, k->sig, &k->info);
}

static long kill_tasks(int pid, int sig, task_t *sender, int code) {
  if (sig < 0 || sig > NSIG)
    return -EINVAL;
  struct kill_target k = {
      .pid = pid,
      .sig = sig,
      .sender = sender,
      .info = {.code = code, .pid = sender ? (int32_t)sender->id.pid : 0},
  };
  task_foreach(kill_one, &k);
  return k.found ? 0 : -ESRCH;
}

long signal_send_pid(int pid, int sig) {
  return pid > 0 ? kill_tasks(pid, sig, NULL, SI_KERNEL) : -ESRCH;
}

void signal_send_pgrp(uint32_t pgrp, int sig) {
  if (pgrp)
    kill_tasks(-(int)pgrp, sig, NULL, SI_KERNEL);
}

long sys_kill(int pid, int sig) {
  return kill_tasks(pid, sig, get_current_task(), SI_USER);
}

/* One task is one thread here: a thread id is a pid. */
long sys_tkill(int tid, int sig) {
  if (tid <= 0)
    return -EINVAL;
  return kill_tasks(tid, sig, get_current_task(), SI_USER);
}

long sys_tgkill(int tgid, int tid, int sig) {
  if (tgid <= 0 || tid != tgid)
    return tid <= 0 ? -EINVAL : -ESRCH;
  return sys_tkill(tid, sig);
}

/* -- Receiving ---------------------------------------------------------- */

bool signal_pending(void) {
  task_t *task = get_current_task();
  return task && task->abi && sig_pending(&task->signals);
}

static void sleep_while_stopped(task_t *task) {
  for (;;) {
    task_prepare_wait();
    if (!sig_stopped(&task->signals)) {
      task_cancel_wait();
      return;
    }
    task_wait(0);
  }
}

void signal_return(registers_t *regs, bool from_syscall) {
  task_t *task = get_current_task();
  if (!task || !task->abi)
    return;

  /* After a preemptive switch, @p regs is the next task's frame copied
   * onto the previous task's kernel stack (task_state_load). Stopping here
   * would sleep on a stack that isn't ours, and a handler's frame could
   * race the previous task reusing it: the switched-in task sees its
   * signals on its next kernel entry instead (a tick at most). */
  uint64_t frame = (uint64_t)regs;
  if (frame < task->exec.rsp0 ||
      frame >= task->exec.rsp0 + task->exec.rsp0_size)
    return;

  for (;;) {
    int r = sig_next(&task->signals, regs, from_syscall);
    if (r == 0)
      return;
    if (r > 0)
      task_exit_status(r); /* killed: the wait status is the signal */

    /* Stopped (Ctrl-Z): the parent hears of it, and we sleep until a
     * SIGCONT or SIGKILL. Then whatever is pending is looked at again. */
    task_notify_parent(task, CLD_STOPPED, -r);
    sleep_while_stopped(task);
  }
}

void signal_fault(registers_t *regs) {
  int sig;
  switch (regs->int_no) {
  case 0:  /* divide error */
  case 16: /* x87 */
  case 19: /* SIMD */
    sig = SIGFPE;
    break;
  case 1: /* debug */
  case 3: /* int3 */
    sig = SIGTRAP;
    break;
  case 6: /* invalid opcode */
    sig = SIGILL;
    break;
  case 17: /* alignment check */
    sig = SIGBUS;
    break;
  default: /* page fault, #GP, ... */
    sig = SIGSEGV;
    break;
  }

  uint64_t addr = regs->rip;
  if (regs->int_no == 14)
    asm volatile("mov %%cr2, %0" : "=r"(addr));

  struct sig_info info = {.code = sig == SIGSEGV ? SEGV_MAPERR : SI_KERNEL,
                          .addr = addr};
  sig_force(&get_current_task()->signals, sig, &info);
}
