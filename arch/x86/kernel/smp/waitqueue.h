/**
 * @file waitqueue.h
 * @brief Wait queue for task blocking and waking
 */

#pragma once

#include <smp/scheduler.h>
#include <smp/spinlock.h>

/* ── Wait queue structure ─────────────────────────────────────────────── */

/**
 * @brief Wait queue for synchronizing task sleep/wakeup
 */
typedef struct wait_queue {
	task_t *tasks[MAX_TASKS];
	size_t count;
	spinlock_t lock;
} wait_queue_t;

/* ── Public functions ─────────────────────────────────────────────────── */

/**
 * @brief Initialize a wait queue
 * @param wq Pointer to wait queue
 */
void waitqueue_init(wait_queue_t *wq);

/**
 * @brief Sleep on a wait queue (blocks current task)
 * @param wq Pointer to wait queue
 */
void waitqueue_sleep(wait_queue_t *wq);

/**
 * @brief Wake all tasks sleeping on a wait queue
 * @param wq Pointer to wait queue
 */
void waitqueue_wake_all(wait_queue_t *wq);
