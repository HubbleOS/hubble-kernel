#pragma once

#include <smp/scheduler.h>
#include <smp/spinlock.h>

typedef struct wait_queue
{
	task_t *tasks[MAX_TASKS];
	size_t count;
	spinlock_t lock;
} wait_queue_t;

void waitqueue_wake_all(wait_queue_t *wq);
void waitqueue_init(wait_queue_t *wq);
void waitqueue_sleep(wait_queue_t *wq);
