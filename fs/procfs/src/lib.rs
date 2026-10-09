//! procfs: /proc for Hubble, as a loadable Rust module.
//!
//! Mounted at /proc by module init. Each regular file's text is
//! generated on open (see files.rs); the root is the only directory.

#![no_std]

extern crate alloc;

mod files;
mod heap;
mod kernel;

use alloc::boxed::Box;
use alloc::string::String;
use alloc::vec::Vec;
use core::ffi::{CStr, c_char, c_int, c_void};
use core::ptr;

use kernel::*;

/* -- Module metadata (read by kernel/module.c) ---------------------------- */

#[unsafe(link_section = ".module.init")]
#[used]
static MODULE_INIT: extern "C" fn() -> c_int = procfs_init;

#[unsafe(link_section = ".module.name")]
#[used]
static MODULE_NAME: [u8; 7] = *b"procfs\0";

/// Named (no_mangle) so the module link can use it as the --gc-sections
/// root; the symbol is made local in the finished .ko.
#[unsafe(no_mangle)]
extern "C" fn procfs_init() -> c_int {
    let fs = Box::leak(Box::new(VfsFs {
        fs_type: FS_PROC,
        fs: ptr::null_mut(),
        mount: None,
        unmount: None,
        create_file: None,
        open: Some(procfs_open),
        read: Some(procfs_read),
        write: Some(procfs_write),
        mmap: None,
        mkdir: None,
        unlink: None,
        close: Some(procfs_close),
        readdir: Some(procfs_readdir),
        truncate: Some(procfs_truncate),
        rename: None,
    }));

    if unsafe { vfs_mount_fs(c"/proc".as_ptr(), fs) } {
        unsafe { printk(c"<8>procfs: mounted at /proc\n".as_ptr()) };
        0
    } else {
        unsafe { printk(c"<3>procfs: mount failed\n".as_ptr()) };
        -1
    }
}

/* -- VFS operations -------------------------------------------------------- */

/// Path relative to /proc with surrounding slashes removed ("" = root).
unsafe fn relative(path: *const c_char) -> &'static [u8] {
    if path.is_null() {
        return b"";
    }
    let bytes = unsafe { CStr::from_ptr(path) }.to_bytes();
    let start = bytes.iter().position(|&c| c != b'/').unwrap_or(bytes.len());
    let end = bytes
        .iter()
        .rposition(|&c| c != b'/')
        .map_or(start, |i| i + 1);
    &bytes[start..end.max(start)]
}

unsafe extern "C" fn procfs_open(fs: *mut VfsFs, path: *const c_char) -> *mut VfsNode {
    let name = unsafe { relative(path) };

    let (is_dir, contents) = if name.is_empty() {
        (true, None)
    } else if let Some(file) = files::find(name) {
        let mut text = String::new();
        (file.generate)(&mut text);
        (false, Some(Box::new(text.into_bytes())))
    } else {
        return ptr::null_mut();
    };

    let size = contents.as_ref().map_or(0, |c| c.len() as u32);
    let mut node = Box::new(VfsNode {
        name: [0; 256],
        is_dir,
        size,
        mode: 0,
        pos: 0,
        fs_node: contents.map_or(ptr::null_mut(), |c| Box::into_raw(c) as *mut c_void),
        fs,
    });
    for (dst, &src) in node.name.iter_mut().zip(name.iter().take(255)) {
        *dst = src as c_char;
    }
    Box::into_raw(node)
}

unsafe extern "C" fn procfs_read(file: *mut VfsFile, buf: *mut c_void, size: u32) -> c_int {
    let Some(file) = (unsafe { file.as_mut() }) else {
        return -1;
    };
    let node = unsafe { &*file.node };
    if node.fs_node.is_null() {
        return -1; // directory
    }
    let data = unsafe { &*(node.fs_node as *const Vec<u8>) };

    let start = (file.pos as usize).min(data.len());
    let n = (size as usize).min(data.len() - start);
    unsafe { ptr::copy_nonoverlapping(data.as_ptr().add(start), buf as *mut u8, n) };
    file.pos += n as u32;
    n as c_int
}

/// The /proc file behind an open node.
unsafe fn node_file(file: *mut VfsFile) -> Option<&'static files::ProcFile> {
    let file = unsafe { file.as_ref() }?;
    let node = unsafe { file.node.as_ref() }?;
    let len = node
        .name
        .iter()
        .position(|&c| c == 0)
        .unwrap_or(node.name.len());
    let name = unsafe { core::slice::from_raw_parts(node.name.as_ptr() as *const u8, len) };
    files::find(name)
}

unsafe extern "C" fn procfs_write(file: *mut VfsFile, buf: *const c_void, size: u32) -> c_int {
    let Some(write) = (unsafe { node_file(file) }).and_then(|f| f.write) else {
        return -EROFS;
    };
    let data = unsafe { core::slice::from_raw_parts(buf as *const u8, size as usize) };
    if write(data) { size as c_int } else { -EINVAL }
}

/// `echo 5 > /proc/loglevel` opens with O_TRUNC; writable files accept
/// it and ignore it, as Linux's /proc does.
unsafe extern "C" fn procfs_truncate(file: *mut VfsFile, _size: u32) -> c_int {
    match unsafe { node_file(file) } {
        Some(f) if f.write.is_some() => 0,
        _ => -EROFS,
    }
}

unsafe extern "C" fn procfs_close(file: *mut VfsFile) -> c_int {
    let Some(file) = (unsafe { file.as_mut() }) else {
        return -1;
    };
    if file.node.is_null() {
        return 0;
    }
    let node = unsafe { Box::from_raw(file.node) };
    if !node.fs_node.is_null() {
        drop(unsafe { Box::from_raw(node.fs_node as *mut Vec<u8>) });
    }
    file.node = ptr::null_mut();
    0
}

unsafe extern "C" fn procfs_readdir(_fs: *mut VfsFs, path: *const c_char) -> Directory {
    let mut dir = Directory {
        entries: ptr::null_mut(),
        count: 0,
        free_entries: Some(procfs_free_entries),
    };
    if !unsafe { relative(path) }.is_empty() {
        return dir; // only the root is a directory
    }

    let entries: Box<[Entry]> = files::FILES
        .iter()
        .map(|f| Entry {
            cluster: 0,
            name: f.name.as_ptr(),
            is_dir: false,
        })
        .collect();
    dir.count = entries.len() as c_int;
    dir.entries = Box::into_raw(entries) as *mut Entry;
    dir
}

/// Names point at static strings; only the array itself is owned.
unsafe extern "C" fn procfs_free_entries(dir: *mut Directory) -> bool {
    let Some(dir) = (unsafe { dir.as_mut() }) else {
        return false;
    };
    if !dir.entries.is_null() {
        let slice = ptr::slice_from_raw_parts_mut(dir.entries, dir.count as usize);
        drop(unsafe { Box::from_raw(slice) });
    }
    dir.entries = ptr::null_mut();
    dir.count = 0;
    true
}

/* -- Panic ------------------------------------------------------------------ */

#[panic_handler]
fn panic(info: &core::panic::PanicInfo) -> ! {
    if let Some(loc) = info.location() {
        let file = loc.file();
        unsafe {
            printk(
                c"<0>procfs: panic at %.*s:%u\n".as_ptr(),
                file.len() as c_int,
                file.as_ptr(),
                loc.line(),
            )
        };
    } else {
        unsafe { printk(c"<0>procfs: panic\n".as_ptr()) };
    }
    loop {
        unsafe { core::arch::asm!("cli", "hlt") };
    }
}
