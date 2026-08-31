/**
 * @file task.h
 * @brief Task Control Block (TCB) and task state definitions
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <fs/vfs/vfs.h>
#include <mm/map/vm_map.h>

/* -- Constants ---------------------------------------------------------- */

#define MAX_FDS 64

/* -- Task state enumeration -------------------------------------------- */

/**
 * @brief Possible states for a task
 */
typedef enum {
  TASK_READY,
  TASK_RUNNING,
  TASK_BLOCKED,
  TASK_SLEEPING,
  TASK_ZOMBIE,
  TASK_DEAD,
  TASK_UNINTERRUPTIBLE
} task_state_t;

/* -- Signal state enumeration ------------------------------------------ */

/**
 * @brief Signal handling states
 */
typedef enum {
  SIG_BLOCKED,
  SIG_UNBLOCKED,
  SIG_DIED,
  SIG_KILLED,
  SIG_IGNORED
} signals_t;

/* -- File descriptor type enumeration ---------------------------------- */

/**
 * @brief File descriptor types
 */
typedef enum { FD_PIPE_READ, FD_PIPE_WRITE, FD_FILE, FD_DEV, FD_MAX } fd_type_t;

/* -- File descriptor entry --------------------------------------------- */

/**
 * @brief A single file descriptor entry
 */
typedef struct {
  void *data;
  int type;
  int flags;
} fd_entry_t;

/* -- CPU context ------------------------------------------------------- */

/**
 * @brief CPU register context saved during context switch
 */
typedef struct __attribute__((packed)) {
  uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
  uint64_t rdi, rsi, rbp, unused, rbx, rdx, rcx, rax;
  uint64_t rsp;
  uint64_t rip;
  uint64_t rflags;
  uint64_t cs;
  uint64_t ss;
  uint64_t ds, es, fs, gs;
  void *fpu_state;
} cpu_context_t;

/* -- Task Control Block ------------------------------------------------ */

/**
 * @brief Task Control Block (TCB)
 */

// ---- Schedualer state ----
typedef struct {
  uint32_t priority;
  uint64_t time_slice;
  uint64_t time_slice_max;
  uint64_t total_runtime;
  uint64_t last_scheduled;
  uint8_t cpu;
} task_sched_t;

// ---- Exec state ----
typedef struct {
  cpu_context_t context;
  bool context_saved;
  bool in_syscall;
  uint64_t in_syscall_rsp;
  uint32_t rsp0_size;
  uint64_t rsp0;
} task_exec_t;

// ---- Identity state ----
typedef struct {
  uint32_t pid;
  uint32_t tid;
  char name[32];
} task_id_t;

// ---- Memory state ----
typedef struct {
  uint64_t *page_table;
  vm_map_t *vm_map;
  uint64_t heap_start;
  uint64_t heap_end;
  uint64_t fs_base;
} task_mm_t;

// ---- Stacks state ----
typedef struct {
  uint64_t kernel_stack;
  uint64_t user_stack;
  size_t stack_size;
  bool userspace;
} task_stacks_t;

// ---- Entry state ----
typedef struct {
  void (*entry_point)(void *arg);
  void *entry_arg;
} task_entry_t;

typedef struct {
  fd_entry_t fds[MAX_FDS];
} task_fdtable_t;

// ---- Signal state ----
typedef struct {
  uint16_t signal;
  void *signal_handlers;
} task_signals_t;

// ---- Linkage state ----
typedef struct {
  task_state_t state;
  int exit_code;
  uint16_t lock;
  uint8_t spinlocks;

  struct task_t *next, *prev;
  struct task_t *parent, *children, *sibling;
} task_linkage_t;

typedef struct task {
  task_id_t id;
  task_sched_t sched;
  task_exec_t exec;
  task_mm_t mm;
  task_stacks_t stacks;
  task_entry_t entry;
  task_fdtable_t fdtable;
  task_signals_t signals;
  task_linkage_t linkage;
} task_t;
