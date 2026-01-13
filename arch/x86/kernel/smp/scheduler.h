#pragma once

#include <stdint.h>
#include <stddef.h>
#include <smp/spinlock.h>
#include <interrupt/interrupt.h>

#include "task.h"
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

void lapic_timer_handler(registers_t *regs);
void scheduler_init(void);
task_t *get_current_task(void);
void scheduler_add_task(task_t *task);

void task_wake(task_t *task);
void task_sleep(void);
task_t *_task_create_with_arg(void (*entry_point)(void *), void *entry_arg, uint32_t priority);
task_t *_task_create_no_arg(void (*entry_point)(void), uint32_t priority);

// Macro that selects the right function based on arguments
#define task_create(...) _task_create_select(__VA_ARGS__, _task_create_with_arg, _task_create_no_arg)(__VA_ARGS__)
#define _task_create_select(_1, _2, _3, NAME, ...) NAME
