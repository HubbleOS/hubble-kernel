
#include <smp/scheduler.h>
#include <smp/spinlock.h>

typedef struct wait_queue
{
	task_t *tasks[MAX_TASKS];
	size_t count;
	spinlock_t lock;
} wait_queue_t;

// Initialize wait queue
void waitqueue_init(wait_queue_t *wq)
{
	wq->count = 0;
	wq->lock = SPINLOCK_INIT("waitqueue");
}

// Sleep on wait queue
void waitqueue_sleep(wait_queue_t *wq)
{
	task_t *current = get_current_task();

	spinlock_acquire(&wq->lock);

	// Add to wait queue
	wq->tasks[wq->count++] = current;

	// Mark as blocked
	current->state = TASK_BLOCKED;

	spinlock_release(&wq->lock);

	while (current->state == TASK_BLOCKED)
	{
		asm volatile("pause");
		asm volatile("hlt");
	}
}

// Wake all tasks on wait queue
void waitqueue_wake_all(wait_queue_t *wq)
{
	spinlock_acquire(&wq->lock);

	for (size_t i = 0; i < wq->count; i++)
	{
		wq->tasks[i]->state = TASK_READY;
	}

	wq->count = 0;
	spinlock_release(&wq->lock);
}
