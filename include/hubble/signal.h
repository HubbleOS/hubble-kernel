/**
 * @file signal.h
 * @brief Signals: numbers, the per-task state, and the kernel's API
 *
 * The signal core (state, delivery, the signal frame and most signal
 * syscalls) is Rust: rust/src/signal.rs and sigframe.rs. C keeps the parts
 * that touch the scheduler and the entry code (kernel/signal.c).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGWINCH 28
#define NSIG 64

/* si_code values */
#define SI_USER 0
#define SI_KERNEL 0x80
#define CLD_EXITED 1
#define CLD_KILLED 2
#define CLD_STOPPED 5
#define CLD_CONTINUED 6
#define SEGV_MAPERR 1

/* wait4 options */
#define WNOHANG 1
#define WUNTRACED 2
#define WCONTINUED 8

/**
 * A task's signal state: Rust's SigState, which C only keeps room for.
 * Zeroed it is valid: default handlers, nothing blocked or pending.
 * TASK_SIGNALS_WORDS must match TASK_SIGNALS_SIZE in rust/src/signal.rs.
 */
#define TASK_SIGNALS_WORDS 464
typedef struct {
  uint64_t opaque[TASK_SIGNALS_WORDS];
} task_signals_t;

/** What a sender tells the receiver (Rust: SigInfo). */
struct sig_info {
  int32_t code;   /* SI_USER, CLD_*, SEGV_MAPERR */
  int32_t pid;    /* the sender, or the child for SIGCHLD */
  int32_t status; /* SIGCHLD: exit code or signal */
  int32_t _pad;
  uint64_t addr; /* faults: the address */
};

struct task;
typedef struct registers registers_t;

/* -- Rust (rust/src/signal.rs) ------------------------------------------ */

void sig_queue(task_signals_t *s, int sig, const struct sig_info *info);
void sig_force(task_signals_t *s, int sig, const struct sig_info *info);
bool sig_pending(task_signals_t *s);
bool sig_stopped(task_signals_t *s);
bool sig_nocldstop(task_signals_t *parent);
void sig_fork(task_signals_t *child, task_signals_t *parent);
void sig_exec(task_signals_t *s);
int sig_report(task_signals_t *child, int options, bool take);
int sig_next(task_signals_t *s, registers_t *regs, bool from_syscall);
void sig_set_restart(task_signals_t *s, uint64_t deadline_ns);
uint64_t sig_restart(task_signals_t *s);

/* -- C (kernel/signal.c) ------------------------------------------------- */

/** Send @p sig to @p task (a user task) and wake it to handle it. */
void signal_send(struct task *task, int sig, const struct sig_info *info);

/** Send @p sig to every task in process group @p pgrp (from the kernel:
 *  Ctrl-C, terminal resizes). */
void signal_send_pgrp(uint32_t pgrp, int sig);

/** Whether the calling task has a signal that should interrupt a blocking
 *  call (it then returns -ERESTARTSYS or -EINTR). */
bool signal_pending(void);

/**
 * The last step before returning to user space with @p regs: handle
 * pending signals (run a handler, stop, or die).
 * @param from_syscall @p regs ends a syscall (its result is in rax)
 */
void signal_return(registers_t *regs, bool from_syscall);

/** A user-mode CPU exception: the signal it means for the task. */
void signal_fault(registers_t *regs);
