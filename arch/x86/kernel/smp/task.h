#pragma once

#include <stdint.h>
#include <stddef.h>
#include <mm/map/vm_map.h>
#include <fs/vfs/vfs.h>

#define MAX_FDS 64

// Task states
typedef enum
{
	TASK_READY,
	TASK_RUNNING,
	TASK_BLOCKED,
	TASK_SLEEPING,
	TASK_ZOMBIE,
	TASK_DEAD,
	TASK_UNINTERRUPTIBLE
} task_state_t;

typedef enum
{
	SIG_BLOCKED,
	SIG_UNBLOCKED,
	SIG_DIED,
	SIG_KILLED,
	SIG_IGNORED
} signals_t;

typedef enum
{
	FD_PIPE_READ,
	FD_PIPE_WRITE,
	FD_FILE,
	FD_DEV,
	FD_MAX
} fd_type_t;

typedef struct
{
	void *data; // pipe_t*, file_t*, etc
	int type;   // FD_PIPE_READ, FD_PIPE_WRITE, FD_FILE
	int flags;
} fd_entry_t;

// CPU context saved during context switch
typedef struct __attribute__((packed))
{
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
// Task Control Block (TCB)
typedef struct task
{
	// Identification
	uint32_t pid;
	uint32_t tid;
	char name[32];

	// State
	task_state_t state;
	int exit_code;

	// Scheduling
	uint8_t cpu;		 // Which CPU is running this
	uint32_t priority;	 // 0 = highest
	uint64_t time_slice;	 // Remaining time slice
	uint64_t time_slice_max; // Maximum time slice
	uint64_t total_runtime;	 // Total CPU time used
	uint64_t last_scheduled; // Last time scheduled
	uint16_t signal;	 // Pending signals

	uint16_t lock;

	// CPU context
	cpu_context_t context;
	bool context_saved;
	bool in_syscall;
	uint64_t in_syscall_rsp;
	// Syscall stack
	uint32_t rsp0_size;
	uint64_t rsp0;

	// Function to run
	void (*entry_point)(void *arg);
	void *entry_arg;

	// Memory management
	uint64_t *page_table;  // CR3 value
	uint64_t kernel_stack; // Kernel stack base
	uint64_t user_stack;   // User stack base
	size_t stack_size;

	// Virtual memory
	vm_map_t *vm_map;

	// Linked list pointers
	struct task *next;
	struct task *prev;

	// Parent/child relationships
	struct task *parent;
	struct task *children;
	struct task *sibling;

	// File descriptors, signals, etc.
	fd_entry_t fds[MAX_FDS];

	void *signal_handlers;

	uint8_t spinlocks;

} task_t;
