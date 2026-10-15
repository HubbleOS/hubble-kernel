//! /proc/<pid>: one directory per task, holding PID_FILES.
//!
//! Task data is never kept: every open and readdir asks the scheduler
//! again (task_get / task_snapshot copy it out under the runqueue locks),
//! so a task that exited in between is simply not found (ENOENT).

use alloc::boxed::Box;
use alloc::string::String;
use alloc::vec::Vec;
use core::ffi::CStr;
use core::fmt::Write;

use crate::files::as_text;
use crate::kernel::*;

/// A copy of one task, taken under its runqueue lock.
pub struct Proc {
    pub task: Task,
    /// The parent's pid (0 without one), resolved by C under the lock:
    /// task.linkage.parent may already be freed when Rust looks at it.
    pub ppid: u32,
}

/// A file inside /proc/<pid>/, generated from that task's copy.
pub struct PidFile {
    pub name: &'static CStr,
    pub generate: fn(&Proc, &mut String),
}

pub static PID_FILES: &[PidFile] = &[
    PidFile {
        name: c"status",
        generate: status,
    },
    PidFile {
        name: c"comm",
        generate: comm,
    },
    PidFile {
        name: c"stat",
        generate: stat,
    },
];

pub fn parse_pid(s: &[u8]) -> Option<u32> {
    core::str::from_utf8(s).ok()?.parse().ok()
}

/// The task with this pid right now, or None if it does not exist (anymore).
/// Boxed: a Task is 1.8 KB, too much to move around a 16 KB kernel stack.
pub fn task(pid: u32) -> Option<Box<Proc>> {
    let mut proc = Box::<Proc>::new_uninit();
    let p = proc.as_mut_ptr();
    let mut ppid = 0;
    // C writes the task straight into the box: no 1.8 KB copy on the stack.
    if !unsafe { task_get(pid, &raw mut (*p).task, &mut ppid) } {
        return None;
    }
    unsafe {
        (&raw mut (*p).ppid).write(ppid);
        // Both fields are written now.
        Some(proc.assume_init())
    }
}

/// The pid of the task calling us: /proc/self.
pub fn current_pid() -> Option<u32> {
    // Our own task can't exit while it is running this code, so reading
    // through the live pointer is safe, unlike for any other task.
    let task = unsafe { get_current_task().as_ref() }?;
    Some(task.id.pid)
}

/// The pids of every task right now.
pub fn snapshot() -> Vec<u32> {
    let mut pids = Vec::with_capacity(1024);
    let n = unsafe { task_snapshot(pids.as_mut_ptr(), pids.capacity()) };
    // task_snapshot wrote the first n entries.
    unsafe { pids.set_len(n) };
    pids
}

pub fn state_char(state: u32) -> char {
    match state {
        TASK_READY | TASK_RUNNING => 'R',
        TASK_BLOCKED | TASK_SLEEPING => 'S',
        TASK_ZOMBIE => 'Z',
        TASK_DEAD => 'X',
        TASK_UNINTERRUPTIBLE => 'D',
        _ => '?',
    }
}

fn status(p: &Proc, out: &mut String) {
    let t = &p.task;
    let _ = writeln!(out, "Name:\t{}", as_text(&t.id.name));
    let _ = writeln!(out, "State:\t{}", state_char(t.linkage.state));
    let _ = writeln!(out, "Pid:\t{}", t.id.pid);
    let _ = writeln!(out, "PPid:\t{}", p.ppid);
}

fn comm(p: &Proc, out: &mut String) {
    let _ = writeln!(out, "{}", as_text(&p.task.id.name));
}

fn stat(p: &Proc, out: &mut String) {
    let t = &p.task;
    let _ = writeln!(
        out,
        "{} {} {} {}",
        t.id.pid,
        as_text(&t.id.name),
        state_char(t.linkage.state),
        p.ppid
    );
}
