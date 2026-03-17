#include "task.h"
#include "scheduler.h"
#include <mm/kmalloc.h>
#include <string.h>
#include <apic/apic.h>
#include <interrupt/interrupt.h>
#include <printk.h>
#include <hpet/hpet.h>
#include "io.h"

#include <smp/smp.h>

// Per-CPU current task

static __thread task_t *current_task[MAX_CPUS];

static cpu_runqueue_t runqueues[MAX_CPUS];
static bool initialized = false;

bool is_scheduler_initialized(void)
{
	return initialized;
}

task_t *get_next_task(uint8_t cpu_id);
void scheduler_add_task(task_t *task);
void task_wrapper(void);
void schedule(void);
void save_context(task_t *current, registers_t *regs);
void free_context(task_t *task);

#include <mm/slab.h>

static slab_cache_t *fpu_cache = NULL;

// Initialize a new task
task_t *_task_create_with_arg(void (*entry_point)(void *), void *entry_arg, uint32_t priority)
{
	printk("Creating task\n");
	task_t *task = kmalloc(sizeof(task_t), GFP_KERNEL);
	if (!task)
		return NULL;

	memset(task, 0, sizeof(task_t));

	// Assign PID
	printk("Assigning PID\n");
	static uint32_t next_pid = 1;
	task->pid = __atomic_fetch_add(&next_pid, 1, __ATOMIC_SEQ_CST);

	// Set state
	printk("Setting state\n");
	task->state = TASK_READY;
	task->priority = priority;
	task->time_slice = 10; // 10 ticks

	// Allocate kernel stack
	printk("Allocating kernel stack\n");
	task->stack_size = 16384; // 16 KB
	task->kernel_stack = (uint64_t)kmalloc(task->stack_size, GFP_KERNEL);
	if (!task->kernel_stack)
	{
		kfree(task);
		return NULL;
	}

	// Set up initial stack
	printk("Setting up initial stack\n");
	uint64_t *stack_top = (uint64_t *)(task->kernel_stack + task->stack_size);

	// Push initial values onto stack
	printk("Pushing initial values onto stack\n");
	// *(--stack_top) = 0x202;			 // RFLAGS (interrupts enabled)
	// *(--stack_top) = 0x08;			 // CS
	// *(--stack_top) = (uint64_t)task_wrapper; // RIP

	// Initialize context
	printk("Initializing context\n");
	task->context.rsp = (uint64_t)stack_top;
	task->context.rip = (uint64_t)task_wrapper;
	task->context.cs = 0x08;
	task->context.ss = 0x10;
	task->context.ds = 0x10;
	task->context.es = 0x10;
	task->context.fs = 0x10;
	task->context.gs = 0x10;
	task->context.rflags = 0x202;

	task->entry_point = entry_point;
	task->entry_arg = entry_arg;

	task->context.rdi = (uint64_t)task;

	// print all
	printk("RSP: %p\n", task->context.rsp);
	printk("RIP: %p\n", task->context.rip);
	printk("CS: %p\n", task->context.cs);
	printk("SS: %p\n", task->context.ss);
	printk("DS: %p\n", task->context.ds);
	printk("ES: %p\n", task->context.es);
	printk("FS: %p\n", task->context.fs);
	printk("GS: %p\n", task->context.gs);
	printk("RFLAGS: %p\n", task->context.rflags);

	// Allocate FPU state (512 bytes, 16-byte aligned)
	printk("Allocating FPU state\n");
	// void *fpu_state = kmalloc(512 + 16, GFP_KERNEL);
	fpu_cache = slab_cache_create(512, 16);
	// task->context.fpu_state = (void *)(((uintptr_t)fpu_state + 15) & ~0xF);
	task->context.fpu_state = slab_cache_alloc(fpu_cache);

	if (task->context.fpu_state)
	{
		// Initialize with default FPU state
		printk("Initializing FPU state\n");
		asm volatile("fxsave %0" : "=m"(*(char *)task->context.fpu_state));
	}
	printk("FPU state: %p\n", task->context.fpu_state);

	// Allocate page table (or share kernel page table)

	// get kernel page table
	printk("Getting kernel page table\n");
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	task->page_table = (uint64_t *)cr3; // Or create new one
	printk("Page table: %p\n", task->page_table);

	task->signal = 0;

	printk("Task created\n");
	return task;
}

// Create a new idle task
task_t *_task_create_no_arg(void (*entry_point)(void), uint32_t priority)
{
	// void *arg = NULL;
	return _task_create_with_arg((void (*)(void *))entry_point, NULL, priority);
}

void task_exit(int exit_code)
{
	uint8_t cpu_id = lapic_get_id();
	task_t *task = get_current_task();

	outb(0x3f8, 'E');

	if (!task)
		return;

	task->exit_code = exit_code;
	task->state = TASK_DEAD;

	// Remove task from runqueue
	spinlock_acquire(&runqueues[cpu_id].lock);

	// Find task in queue
	size_t task_index = 0;
	for (size_t i = 0; i < runqueues[cpu_id].count; i++)
	{
		if (runqueues[cpu_id].queue[i] == task)
		{
			task_index = i;
			break;
		}
	}

	// Shift remaining tasks down
	for (size_t i = task_index; i < runqueues[cpu_id].count - 1; i++)
	{
		runqueues[cpu_id].queue[i] = runqueues[cpu_id].queue[i + 1];
	}

	runqueues[cpu_id].count--;
	spinlock_release(&runqueues[cpu_id].lock);
	free_context(task);
	current_task[cpu_id] = NULL;

	// Force immediate reschedule to idle or another task
	schedule();

	// Should never reach here
	while (1)
		asm volatile("hlt");
}

void task_sleep(void)
{
	uint8_t cpu_id = lapic_get_id();
	task_t *current = current_task[cpu_id];

	if (!current)
		return;

	outb(0x3f8, 'S');

	// Mark as blocked
	current->state = TASK_BLOCKED;

	// Force reschedule
	schedule();
}

void task_wake(task_t *task)
{
	if (!task || task->state != TASK_BLOCKED)
		return;

	// Mark as ready
	task->state = TASK_READY;
}

__attribute__((noreturn)) void task_wrapper(void)
{
	register task_t *current asm("rdi");

	current->entry_point(current->entry_arg);

	task_exit(0);
	__builtin_unreachable();
}

void task_kill_by_task(task_t *task)
{
	task->state = TASK_ZOMBIE;
}
void task_kill_by_pid(uint32_t pid)
{
	// not implemented yet)
}

// Schedule next task on current CPU
void schedule(void)
{
	uint8_t cpu_id = lapic_get_id();
	task_t *old_task = get_current_task();

	// if (old_task && old_task->state == TASK_READY)
	// 	old_task->state = TASK_RUNNING;

	// Get next task from runqueue
	task_t *new_task = get_next_task(cpu_id);

	if (new_task == runqueues[cpu_id].idle_task)
	{

		if (old_task && old_task->state == TASK_RUNNING)
		{
			// outb(0x3f8, 'O');
			return;
		}
		// outb(0x3f8, old_task->state + '0');
		if (!old_task)
		{
			// printk("old_task is NULL\n");
			outb(0x3f8, 'N');
		}
		// if (!old_task)
		// {

		// 	// outb(0x3f8, 'I');
		// }
		// if (old_task && old_task->state == TASK_READY && old_task != runqueues[cpu_id].idle_task)
		// {
		// 	old_task->state = TASK_RUNNING;
		// 	outb(0x3f8, 'P');
		// 	outb(0x3f8, old_task->state + '0');
		// printk("Switching to %p, state p: %p\n", old_task, old_task->state);
		// 	return;
		// }
		// outb(0x3f8, old_task->state == TASK_BLOCKED ? 'B' : 'R');
		// outb(0x3f8, old_task->state + '0');
	}

	if (!new_task || new_task == old_task)
	{
		if (old_task)
		{
			// outb(0x3f8, 'S');
			return;
		}
		// new_task = runqueues[cpu_id].idle_task;
	}
	// outb(0x3f8, 'R');

	// Update states
	if (old_task && (old_task->state == TASK_RUNNING || old_task->state == TASK_BLOCKED || old_task->state == TASK_READY))
	{
		old_task->state = old_task->state == TASK_BLOCKED ? TASK_BLOCKED : TASK_READY;
		old_task->total_runtime += 100;
	}

	new_task->state = TASK_RUNNING;
	new_task->last_scheduled = 0;
	new_task->cpu = cpu_id;
	current_task[cpu_id] = new_task;

	// Switch page tables if different
	if (new_task->page_table != (old_task ? old_task->page_table : NULL))
	{
		asm volatile("mov %0, %%cr3" ::"r"(new_task->page_table));
	}
	// print rax
	// printk("RAX: %p\n", new_task->context.rax);
	// print rdi
	// printk("enter point from rdi: %p\n", new_task->context.rdi);
	// Perform context switch
	extern void switch_to_task(cpu_context_t * old, cpu_context_t * new);
	switch_to_task(old_task ? &old_task->context : NULL, &new_task->context);
}

void lapic_timer_handler(registers_t *regs)
{
	if (!initialized)
	{
		return;
	}
	// outb(0x3f8, 'T');
	task_t *current = get_current_task();

	if (current)
	{
		save_context(current, regs);
		current->time_slice--;
		if (current->state == TASK_UNINTERRUPTIBLE)
		{
			if (current->time_slice < 30)
			{
				outb(0x3f8, 'W');
			}
			if (current->time_slice < 60)
			{
				outb(0x3f8, 'K');
				task_exit(-1);
			}
			return;
		}
	}

	if (!current || current->time_slice == 0)
	{
		if (current)
			current->time_slice = 10;

		schedule();

		// __builtin_unreachable();
	}

	if (current->state == TASK_ZOMBIE)
	{
		if (current->spinlocks > 0)
		{
			return;
		}
		task_exit(-1);
	}

	lapic_eoi();
}

void save_context(task_t *current, registers_t *regs)
{
	if (!current)
	{
		return;
	}

	current->context.r15 = regs->r15;
	current->context.r14 = regs->r14;
	current->context.r13 = regs->r13;
	current->context.r12 = regs->r12;
	current->context.r11 = regs->r11;
	current->context.r10 = regs->r10;
	current->context.r9 = regs->r9;
	current->context.r8 = regs->r8;
	current->context.rbp = regs->rbp;
	current->context.rdi = regs->rdi;
	current->context.rsi = regs->rsi;
	current->context.rdx = regs->rdx;
	current->context.rcx = regs->rcx;
	current->context.rbx = regs->rbx;
	current->context.rax = regs->rax;

	current->context.rip = regs->rip;
	current->context.rsp = regs->rsp;
	current->context.rflags = regs->rflags;
	current->context.cs = regs->cs;
	current->context.ss = regs->ss;

	// Save FPU state
	if (current->context.fpu_state)
	{
		asm volatile("fxsave (%0)" ::"r"(current->context.fpu_state) : "memory");
	}
}

void free_context(task_t *task)
{

	// Free kernel stack
	if (task->kernel_stack)
	{
		kfree((void *)task->kernel_stack);
	}

	// free fpu state
	// if (task->context.fpu_state)
	// {
	// 	kfree(task->context.fpu_state);
	// }

	if (task->context.fpu_state)
		slab_cache_free(fpu_cache, task->context.fpu_state);
}

task_t *get_next_task(uint8_t cpu_id)
{
	cpu_runqueue_t *rq = &runqueues[cpu_id];

	spinlock_acquire(&rq->lock);

	if (rq->count == 0)
	{
		spinlock_release(&rq->lock);
		return rq->idle_task;
	}

	// Find next READY task using runqueue's index
	for (size_t i = 0; i < rq->count; i++)
	{
		size_t index = (rq->next_index + i) % rq->count;
		if (rq->queue[index]->state == TASK_READY || rq->queue[index]->state == TASK_ZOMBIE)
		{
			rq->next_index = (index + 1) % rq->count;
			task_t *next = rq->queue[index];
			spinlock_release(&rq->lock);
			return next;
		}
	}

	spinlock_release(&rq->lock);
	return rq->idle_task;
}

task_t *get_current_task(void)
{
	uint8_t cpu_id = lapic_get_id();
	return current_task[cpu_id];
}

// Example: kernel thread entry point
void idle_task(void)
{
	uint16_t count = 0;
	while (1)
	{
		// printk("CPU %d idle, time: %d\n", lapic_get_id(), count++);
		// hpet_delay_ms(1000);
		asm volatile("hlt");
	}
}

// Initialize scheduler
void scheduler_init(void)
{
	printk("Initializing scheduler\n");
	for (int i = 0; i < smp_get_cpu_count(); i++)
	{
		printk("Initializing runqueue for CPU %d\n", i);
		runqueues[i].count = 0;
		runqueues[i].next_index = 0; // Initialize
		// Create idle task for each CPU
		task_t *idle = task_create(idle_task, 255);
		runqueues[i].idle_task = idle;
		current_task[i] = NULL;
	}

	extern void kmain_thread(void);
	task_t *kmain = task_create(kmain_thread, 255);
	scheduler_add_task(kmain);

	initialized = true;
	while (1)
	{
		asm volatile("hlt");
	}
}

void scheduler_add_task(task_t *task)
{
	printk("Adding task %s to runqueue\n", task->name);
	// Find CPU with fewest tasks
	int target_cpu = 0;
	size_t min_load = runqueues[0].count;

	for (int i = 1; i < smp_get_cpu_count(); i++)
	{
		if (runqueues[i].count < min_load)
		{
			min_load = runqueues[i].count;
			target_cpu = i;
		}
	}

	// Add to that CPU's queue
	spinlock_acquire(&runqueues[target_cpu].lock);
	runqueues[target_cpu].queue[runqueues[target_cpu].count++] = task;
	task->cpu = target_cpu;
	spinlock_release(&runqueues[target_cpu].lock);
}
