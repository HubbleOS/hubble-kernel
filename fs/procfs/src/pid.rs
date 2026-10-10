//! /proc/<pid>: one directory per task, holding PID_FILES.
//!
//! Task data is never kept: every open and readdir asks the scheduler
//! again (task_get_info / task_snapshot copy it out under the runqueue
//! locks), so a task that exited in between is simply not found (ENOENT).

use alloc::string::String;
use alloc::vec::Vec;
use core::ffi::CStr;
use core::fmt::Write;
use core::mem::MaybeUninit;

use crate::files::as_text;
use crate::kernel::{TaskInfo, task_get_info, task_snapshot};

/// A file inside /proc/<pid>/, generated from that task's info.
pub struct PidFile {
    pub name: &'static CStr,
    pub generate: fn(&TaskInfo, &mut String),
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
];

pub fn parse_pid(s: &[u8]) -> Option<u32> {
    core::str::from_utf8(s).ok()?.parse().ok()
}

/// The task with this pid right now, or None if it does not exist (anymore).
pub fn task(pid: u32) -> Option<TaskInfo> {
    let mut info = MaybeUninit::<TaskInfo>::uninit();
    if unsafe { task_get_info(pid, info.as_mut_ptr()) } {
        // task_get_info filled it in when it returned true.
        Some(unsafe { info.assume_init() })
    } else {
        None
    }
}

/// Every task right now. On the heap: 1024 entries are 48 KB, three
/// times a kernel stack.
pub fn snapshot() -> Vec<TaskInfo> {
    let mut tasks = Vec::with_capacity(1024);
    let n = unsafe { task_snapshot(tasks.as_mut_ptr(), tasks.capacity()) };
    // task_snapshot wrote the first n entries.
    unsafe { tasks.set_len(n) };
    tasks
}

pub fn state_char(state: u32) -> char {
    match state {
        0 | 1 => 'R', // READY, RUNNING
        2 | 3 => 'S', // BLOCKED, SLEEPING
        4 => 'Z',
        5 => 'X',
        6 => 'D',
        _ => '?',
    }
}

fn status(t: &TaskInfo, out: &mut String) {
    let _ = writeln!(out, "Name:\t{}", as_text(&t.name));
    let _ = writeln!(out, "State:\t{}", state_char(t.state));
    let _ = writeln!(out, "Pid:\t{}", t.pid);
    let _ = writeln!(out, "PPid:\t{}", t.ppid);
}

fn comm(t: &TaskInfo, out: &mut String) {
    let _ = writeln!(out, "{}", as_text(&t.name));
}
