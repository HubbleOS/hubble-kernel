#pragma once

#include <stdint.h>
#include <stddef.h>
#include <smp/spinlock.h>
#include <interrupt/interrupt.h>
#include <smp/task.h>

#define MAX_TASKS 256
#define MAX_CPUS 16

typedef struct
{
	task_t *queue[MAX_TASKS];
	size_t count;
	spinlock_t lock;
	task_t *current;
	uint64_t idle_time;
	task_t *idle_task;
	uint32_t next_index;
} cpu_runqueue_t;

// cpu kill queue
typedef struct
{
	task_t *queue[MAX_TASKS];
	size_t count;
} cpu_killqueue_t;

void lapic_timer_handler(registers_t *regs);
void scheduler_init(void);
task_t *get_current_task(void);
void scheduler_add_task(task_t *task);
bool is_scheduler_initialized(void);
void task_exit(int exit_code);

void task_wake(task_t *task);
void task_sleep(void);

task_t *_task_create_with_arg(void (*entry_point)(void *), void *entry_arg, uint32_t priority, bool userspace);
task_t *_task_create_no_arg(void (*entry_point)(void), uint32_t priority, bool userspace);
void task_map_user_stack(task_t *task, uint64_t *pml4_phys);

#define _task_create_select(_1, _2, _3, _4, NAME) NAME

#define task_create(...)                           \
	_task_create_select(__VA_ARGS__,           \
			    _task_create_with_arg, \
			    _task_create_no_arg)(__VA_ARGS__)

void task_kill_by_task(task_t *task);
void task_kill_by_pid(uint32_t pid);

#define task_kill(...) _task_kill_select(__VA_ARGS__, task_kill_by_pid, task_kill_by_task)(__VA_ARGS__)
#define _task_kill_select(_1, _2, _3, NAME, ...) NAME
