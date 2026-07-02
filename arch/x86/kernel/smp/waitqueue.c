/**
 * @file waitqueue.c
 * @brief Wait queue implementation for task blocking and waking
 */

#include <smp/scheduler.h>
#include <smp/spinlock.h>

#include "waitqueue.h"

/* -- Initialization ----------------------------------------------------- */

/**
 * @brief Initialize a wait queue
 * @param wq Pointer to wait queue
 */
void waitqueue_init(wait_queue_t *wq) {
  wq->count = 0;
  wq->lock = SPINLOCK_INIT("waitqueue");
}

/* -- Sleep / Wake ------------------------------------------------------- */

/**
 * @brief Sleep on a wait queue (blocks current task)
 * @param wq Pointer to wait queue
 */
void waitqueue_sleep(wait_queue_t *wq) {
  task_t *current = get_current_task();

  spinlock_acquire(&wq->lock);

  wq->tasks[wq->count++] = current;

  spinlock_release(&wq->lock);

  task_sleep();
}

/**
 * @brief Wake all tasks sleeping on a wait queue
 * @param wq Pointer to wait queue
 */
void waitqueue_wake_all(wait_queue_t *wq) {
  spinlock_acquire(&wq->lock);

  for (size_t i = 0; i < wq->count; i++) {
    wq->tasks[i]->state = TASK_READY;
    wq->tasks[i]->time_slice = wq->tasks[i]->time_slice_max;
  }

  wq->count = 0;
  spinlock_release(&wq->lock);

  need_resched = true;
}
