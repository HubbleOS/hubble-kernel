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
