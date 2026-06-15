#include "vfs.h"
#include "vfs_standart_struct.h"
#include "dev.h"
#include <stdbool.h>
#include <hubble/string.h>
#include <hubble/printk.h>
#include "higher_half.h"

#include <smp/waitqueue.h>
#include <smp/spinlock.h>
#include <smp/scheduler.h>
#include <smp/task.h>

// VFS_Node *dev_vfs_open_device(VFS_FS *fs, const char *path);
// VFS_Node *dev_vfs_create_device(VFS_FS *fs, const char *path);
// int dev_vfs_write_device(VFS_File *file, const void *buf, uint32_t size);
// uint64_t mmap_device(VFS_File *file, uint64_t offset, size_t size);
// int dev_vfs_read_device(VFS_File *file, void *buf, uint32_t size);
// tree structure

#define PIPE_BUFFER_SIZE 4096

#define MAX_PARTS 16

typedef struct VFS_pipes_node VFS_pipes_node;
typedef struct VFS_pipes_tree VFS_pipes_tree;

typedef struct
{
	char *name; // оригінальне ім’я (якщо треба для LFN)
} PathPart;

typedef struct
{
	int count;
	PathPart parts[MAX_PARTS];
} PathParts;

typedef struct
{
	char name[64];
	char buffer[PIPE_BUFFER_SIZE];
	size_t read_pos;
	size_t write_pos;
	size_t count;
	bool write_closed;
	bool read_closed;
	spinlock_t lock;
	wait_queue_t writer;
	wait_queue_t reader;
} pipe_t;

typedef struct VFS_pipes_node
{
	pipe_t *pipe;
	VFS_pipes_node *next;
};

typedef struct VFS_pipes_tree
{
	char name[10];
	VFS_pipes_node *childs_node;
	VFS_pipes_tree *childs_tree;
	VFS_pipes_tree *next;
};

VFS_pipes_tree *VFS_pipes = NULL;

VFS_Node *pipe_vfs_open_pipe(VFS_FS *fs, const char *path);
VFS_Node *pipe_vfs_create_pipe(VFS_FS *fs, const char *path);
int pipe_write(VFS_File *file, const void *buf, uint32_t size);
int pipe_read(VFS_File *file, void *buf, uint32_t size);

PathParts format_pipe_path(const char *in)
{
	printk(KERN_INFO "Formatting folder path: %s\n", in);
	PathParts result = {0};

	while (*in == '/')
		in++;

	while (*in && result.count < MAX_PARTS && *in != '\0')
	{
		const char *end = in;

		while (*end && *end != '/' && *end != '\0')
			end++;

		int len = end - in;

		if (len > 0)
		{
			char *name = kmalloc(len + 1, GFP_KERNEL);
			if (!name)
				return result; // or panic

			memcpy(name, in, len);
			name[len] = '\0';

			result.count++;
			result.parts[result.count - 1].name = name;
		}

		in = end;
		while (*in == '/' && *in != '\0')
			in++;
	}

	return result;
}

// static VFS_device_reg *dev_vfs_devices = NULL;

bool pipe_vfs_init(VFS_FS *fs, VFS_Device *device, uint32_t start_lba)
{
	printk(KERN_INFO "Initializing device fs\n");

	VFS_pipes = kmalloc(sizeof(VFS_pipes_tree), GFP_KERNEL);
	memset(VFS_pipes, 0, sizeof(VFS_pipes_tree));

	fs->create_file = pipe_vfs_create_pipe;
	fs->open = pipe_vfs_open_pipe;
	fs->write = pipe_write;
	fs->read = pipe_read;
	fs->fs = fs;
	return 1;
}

VFS_pipes_tree *pipe_vfs_find_pipe(const char *path, PathParts *parts)
{

	VFS_pipes_tree *current = VFS_pipes;
	if (!current)
	{
		return NULL;
	}
	if (parts->count <= 0)
	{
		return VFS_pipes;
	}

	for (int i = 0; i < parts->count; ++i)
	{
		current = current->childs_tree;
		while (current->next)
		{
			if (strcmp(current->next->name, parts->parts[i].name) == 0)
			{
				current = current->next;
				break;
			}
			current = current->next;
		}
	}
	return current;
}

VFS_pipes_node *pipe_vfs_find_pipe_node(VFS_pipes_tree *pipe_tree, const char *name)
{

	VFS_pipes_node *current = pipe_tree->childs_node;
	if (!current)
	{
		return NULL;
	}
	do
	{
		if (strcmp(name, current->pipe->name) == 0)
		{
			return current;
		}
		current = current->next;
	} while (current);
	return NULL;
}

VFS_Node *pipe_vfs_open_pipe(VFS_FS *fs, const char *path)
{
	VFS_Node *node = kmalloc(sizeof(VFS_Node), GFP_KERNEL);
	PathParts parts = format_pipe_path(path);
	parts.count--;
	VFS_pipes_tree *pipe = pipe_vfs_find_pipe(path, &parts);
	parts.count++;
	if (pipe)
	{
		VFS_pipes_node *pipe_node = pipe_vfs_find_pipe_node(pipe, parts.parts[parts.count - 1].name);
		if (pipe_node)
		{
			node->fs_node = pipe_node->pipe;
			node->fs = fs;
			node->size = 1;
			return node;
		}
	}
	printk(KERN_DEBUG "pipe not found\n");
	return NULL;
}

VFS_Node *pipe_vfs_create_pipe(VFS_FS *fs, const char *path)
{

	VFS_Node *node = kmalloc(sizeof(VFS_Node), GFP_KERNEL);
	PathParts parts = format_pipe_path(path);
	parts.count--;
	VFS_pipes_tree *pipe = pipe_vfs_find_pipe(path, &parts);
	parts.count++;
	if (pipe)
	{
		VFS_pipes_node *pipe_node = kmalloc(sizeof(VFS_pipes_node), GFP_KERNEL);
		if (!pipe_node)
		{
			return NULL;
		}
		memset(pipe_node, 0, sizeof(VFS_pipes_node));
		pipe_node->pipe = kmalloc(sizeof(pipe_t), GFP_KERNEL);
		memset(pipe_node->pipe, 0, sizeof(pipe_t));
		strncpy(pipe_node->pipe->name, parts.parts[parts.count - 1].name, 63);
		pipe_node->next = pipe->childs_node;
		pipe->childs_node = pipe_node;

		waitqueue_init(&pipe_node->pipe->reader);
		waitqueue_init(&pipe_node->pipe->writer);

		node->fs_node = pipe_node->pipe;
		node->fs = fs;
		node->size = PIPE_BUFFER_SIZE;
		return node;
	}
	return NULL;
}

int pipe_write(VFS_File *file, const void *buf, uint32_t size)
{
	pipe_t *pipe = (pipe_t *)file->node->fs_node;
	pipe->write_pos = file->pos % PIPE_BUFFER_SIZE;
	if (!pipe)
	{
		return -1;
	}

	size_t written = 0;
	while (written < size)
	{
		spinlock_acquire(&pipe->lock);

		if (pipe->read_closed)
		{
			spinlock_release(&pipe->lock);
			return -1; // broken pipe
		}

		size_t space = PIPE_BUFFER_SIZE - pipe->count;
		if (space == 0)
		{
			// Buffer full — block
			spinlock_release(&pipe->lock);
			waitqueue_sleep(&pipe->writer);
			continue;
		}

		size_t chunk = (size - written < space) ? size - written : space;
		// printk("pipe write %d\n", chunk);
		for (size_t i = 0; i < chunk; i++)
		{
			const char *cbuf = (const char *)buf;
			pipe->buffer[pipe->write_pos] = cbuf[written + i];
			pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUFFER_SIZE;
		}
		pipe->count += chunk;
		written += chunk;

		// Wake reader if waiting

		waitqueue_wake_all(&pipe->reader);

		spinlock_release(&pipe->lock);
	}
	file->pos += written;
	return written;
}

int pipe_read(VFS_File *file, void *buf, uint32_t size)
{
	pipe_t *pipe = (pipe_t *)file->node->fs_node;
	pipe->read_pos = file->pos % PIPE_BUFFER_SIZE;
	if (!pipe)
	{
		return -1;
	}

	while (1)
	{
		spinlock_acquire(&pipe->lock);

		if (pipe->count == 0)
		{
			if (pipe->write_closed)
			{
				spinlock_release(&pipe->lock);
				return 0; // EOF
			}
			// No data — block
			spinlock_release(&pipe->lock);
			waitqueue_sleep(&pipe->reader);
			continue;
		}

		size_t chunk = (size < pipe->count) ? size : pipe->count;
		char *cbuf = (char *)buf;
		for (size_t i = 0; i < chunk; i++)
		{

			cbuf[i] = pipe->buffer[pipe->read_pos];
			pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUFFER_SIZE;
		}
		pipe->count -= chunk;

		// Wake writer if waiting
		waitqueue_wake_all(&pipe->writer);

		spinlock_release(&pipe->lock);
		file->pos += chunk;
		// printk("pipe read %d, pos %d\n", chunk, file->pos);
		return chunk;
	}
}
