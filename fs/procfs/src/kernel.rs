//! Bindings to the Hubble kernel: VFS types and the exported functions
//! this module uses. Layouts mirror fs/vfs/vfs.h; the offset assertions
//! below fail the build if the C side changes.

use core::ffi::{c_char, c_int, c_void};
use core::mem::{offset_of, size_of};

#[repr(C)]
pub struct VfsNode {
    pub name: [c_char; 256],
    pub is_dir: bool,
    pub size: u32,
    pub mode: u32,
    pub pos: u32,
    pub fs_node: *mut c_void,
    pub fs: *mut VfsFs,
}

#[repr(C)]
pub struct VfsFile {
    pub flags: u32,
    pub pos: u32,
    pub node: *mut VfsNode,
    pub path: *const c_char,
}

#[repr(C)]
pub struct Entry {
    pub cluster: u32,
    pub name: *const c_char,
    pub is_dir: bool,
}

#[repr(C)]
pub struct Directory {
    pub entries: *mut Entry,
    pub count: c_int,
    pub free_entries: Option<unsafe extern "C" fn(*mut Directory) -> bool>,
}

type Unused = Option<unsafe extern "C" fn()>;

#[repr(C)]
pub struct VfsFs {
    pub fs_type: u32,
    pub fs: *mut c_void,
    pub mount: Unused,
    pub unmount: Unused,
    pub create_file: Unused,
    pub open: Option<unsafe extern "C" fn(*mut VfsFs, *const c_char) -> *mut VfsNode>,
    pub read: Option<unsafe extern "C" fn(*mut VfsFile, *mut c_void, u32) -> c_int>,
    pub write: Option<unsafe extern "C" fn(*mut VfsFile, *const c_void, u32) -> c_int>,
    pub mmap: Unused,
    pub mkdir: Unused,
    pub unlink: Unused,
    pub close: Option<unsafe extern "C" fn(*mut VfsFile) -> c_int>,
    pub readdir: Option<unsafe extern "C" fn(*mut VfsFs, *const c_char) -> Directory>,
    pub truncate: Option<unsafe extern "C" fn(*mut VfsFile, u32) -> c_int>,
    pub rename: Unused,
}

/// FileSystemType::FS_PROC in fs/vfs/vfs_standart_struct.h.
pub const FS_PROC: u32 = 6;
pub const GFP_KERNEL: u32 = 0;
pub const EINVAL: c_int = 22;
pub const EROFS: c_int = 30;

const _: () = {
    assert!(size_of::<VfsNode>() == 288);
    assert!(offset_of!(VfsNode, is_dir) == 256);
    assert!(offset_of!(VfsNode, size) == 260);
    assert!(offset_of!(VfsNode, fs_node) == 272);
    assert!(offset_of!(VfsNode, fs) == 280);
    assert!(size_of::<VfsFile>() == 24);
    assert!(offset_of!(VfsFile, pos) == 4);
    assert!(offset_of!(VfsFile, node) == 8);
    assert!(size_of::<Entry>() == 24);
    assert!(offset_of!(Entry, name) == 8);
    assert!(offset_of!(Entry, is_dir) == 16);
    assert!(size_of::<Directory>() == 24);
    assert!(offset_of!(Directory, free_entries) == 16);
    assert!(size_of::<VfsFs>() == 120);
    assert!(offset_of!(VfsFs, fs) == 8);
    assert!(offset_of!(VfsFs, open) == 40);
    assert!(offset_of!(VfsFs, read) == 48);
    assert!(offset_of!(VfsFs, close) == 88);
    assert!(offset_of!(VfsFs, readdir) == 96);
};

unsafe extern "C" {
    pub fn printk(fmt: *const c_char, ...);
    pub fn kmalloc(size: usize, flags: u32) -> *mut c_void;
    pub fn kfree(ptr: *mut c_void);
    pub fn vfs_mount_fs(mountpoint: *const c_char, fs: *mut VfsFs) -> bool;
    pub fn pmm_get_stats(total_bytes: *mut u64, used_bytes: *mut u64);
    pub fn hpet_get_time_ns() -> u64;
    pub fn smp_get_cpu_count() -> u32;
    pub fn printk_get_log(dest: *mut c_char, max_len: usize) -> usize;
    pub fn printk_log_size() -> usize;
    pub fn printk_set_console_level(level: c_int);
    pub fn printk_get_console_level() -> c_int;
}
