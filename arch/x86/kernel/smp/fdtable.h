/**
 * @file fdtable.h
 * @brief A task's file descriptors: allocation, lookup, sharing across
 *        fork, and closing on exec and exit
 *
 * Every descriptor, 0-2 included, is an ordinary slot pointing at a
 * shared file_t (fs/vfs/file.h). A task's table is only touched by the
 * task itself, and by its parent while fork() builds it.
 */
#pragma once

#include <fs/vfs/file.h>
#include <smp/task.h>

/** The open file behind @p fd, or NULL (borrowed: no reference taken). */
file_t *fd_file(task_t *task, int fd);

/**
 * Put @p file in the lowest free slot >= @p min_fd. Takes over one
 * reference to @p file. Returns the descriptor, or -EMFILE.
 */
int fd_install(task_t *task, file_t *file, int min_fd, int fd_flags);

/** Make @p fd refer to @p file (closing what it held). Takes a reference. */
void fd_install_at(task_t *task, int fd, file_t *file, int fd_flags);

/** Close one descriptor. Returns 0 or -EBADF. */
int fd_close(task_t *task, int fd);

/** Give @p child the same open files as @p parent (fork, spawn). */
void fd_copy_table(task_t *child, task_t *parent);

/** Close the descriptors marked FD_CLOEXEC (on a successful exec). */
void fd_close_on_exec(task_t *task);

/** Close every descriptor (on exit). */
void fd_close_all(task_t *task);

/** Open the console as stdin, stdout and stderr of a first process. */
int fd_open_console(task_t *task);
