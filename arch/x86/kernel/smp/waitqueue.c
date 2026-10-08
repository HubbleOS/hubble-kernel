/**
 * @file waitqueue.c
 * @brief Wait queue implementation for task blocking and waking
 */

#include <smp/scheduler.h>
#include <hpet/hpet.h>
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

/* Add @p task once (a task re-waiting after a timeout must not pile up
 * duplicate entries). Caller holds wq->lock. */
static void waitqueue_add_locked(wait_queue_t *wq, task_t *task) {
  for (size_t i = 0; i < wq->count; i++)
    if (wq->tasks[i] == task)
      return;
  if (wq->count < MAX_TASKS)
    wq->tasks[wq->count++] = task;
}

static void waitqueue_remove(wait_queue_t *wq, task_t *task) {
  spinlock_acquire(&wq->lock);
  for (size_t i = 0; i < wq->count; i++) {
    if (wq->tasks[i] == task) {
      wq->tasks[i] = wq->tasks[--wq->count];
      break;
    }
  }
  spinlock_release(&wq->lock);
}

/**
 * @brief Sleep on a wait queue (blocks current task)
 * @param wq Pointer to wait queue
 */
void waitqueue_sleep(wait_queue_t *wq) {
  task_t *current = get_current_task();

  task_prepare_wait();
  spinlock_acquire(&wq->lock);
  waitqueue_add_locked(wq, current);
  spinlock_release(&wq->lock);

  task_wait(0);
}

bool waitqueue_wait_event(wait_queue_t *wq, bool (*cond)(void *), void *arg,
                          uint64_t deadline_ns) {
  task_t *current = get_current_task();

  while (!cond(arg)) {
    if (deadline_ns && hpet_get_time_ns() >= deadline_ns)
      return false;

    task_prepare_wait();
    spinlock_acquire(&wq->lock);
    waitqueue_add_locked(wq, current);
    spinlock_release(&wq->lock);

    if (cond(arg)) {
      task_cancel_wait();
      waitqueue_remove(wq, current);
      return true;
    }

    task_wait(deadline_ns);
    waitqueue_remove(wq, current);
  }
  return true;
}

/**
 * @brief Wake all tasks sleeping on a wait queue
 * @param wq Pointer to wait queue
 */
void waitqueue_wake_all(wait_queue_t *wq) {
  spinlock_acquire(&wq->lock);

  for (size_t i = 0; i < wq->count; i++) {
    task_t *t = wq->tasks[i];
    /* Only blocked tasks: a task between task_prepare_wait() and
     * task_wait() is blocked too, which is what makes waking it safe. */
    if (t->linkage.state == TASK_BLOCKED) {
      t->sched.time_slice = t->sched.time_slice_max;
      t->linkage.state = TASK_READY;
    }
  }

  wq->count = 0;
  spinlock_release(&wq->lock);

  need_resched = true;
}
