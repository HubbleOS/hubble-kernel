
use alloc::string::String;
use alloc::vec::Vec;
use core::ffi::CStr;
use core::fmt::Write;
use core::mem::MaybeUninit;

use crate::files::as_text;
use crate::kernel::{TaskInfo, };


use crate::modules;

pub struct ProcModule {
    pub name: &'static CStr,
    pub generate: fn(&mut String),
}

static PROC_MODULES: &[ProcModule] = &[
    ProcModule {
        name: c"modules",
        generate: modules,
    },
];

pub fn modules(out: &mut String) {
    for m in unsafe { crate::kernel::module_list() } {
        let _ = writeln!(out, "{} {}", as_text(&m.name), m.size);
    }
}

