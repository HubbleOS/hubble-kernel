#pragma once

#include <stdint.h>
#include <stddef.h>
#include <smp/spinlock.h>

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

// CPU context saved during context switch
typedef struct __attribute__((packed))
{
	// General purpose registers
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rdi, rsi, rbp, unused, rbx, rdx, rcx, rax;

	// Stack pointer
	uint64_t rsp;

	// Instruction pointer
	uint64_t rip;

	// Segment selectors
	uint16_t cs, ss, ds, es, fs, gs;

	// Flags
	uint64_t rflags;

	// FPU/SSE state pointer (allocated separately)
	void *fpu_state; // 512 bytes for FXSAVE
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
	uint64_t total_runtime;	 // Total CPU time used
	uint64_t last_scheduled; // Last time scheduled
	uint16_t signal;	 // Pending signals

	uint16_t lock;

	// CPU context
	cpu_context_t context;

	// Function to run
	void (*entry_point)(void *arg);
	void *entry_arg;

	// Memory management
	uint64_t *page_table;  // CR3 value
	uint64_t kernel_stack; // Kernel stack base
	uint64_t user_stack;   // User stack base
	size_t stack_size;

	// Linked list pointers
	struct task *next;
	struct task *prev;

	// Parent/child relationships
	struct task *parent;
	struct task *children;
	struct task *sibling;

	// File descriptors, signals, etc.
	void *files;
	void *signal_handlers;

	uint8_t spinlocks;

} task_t;
