//! Bindings to the Hubble kernel: VFS types and the exported functions
//! this module uses. Layouts mirror fs/vfs/vfs.h; the offset assertions
//! below fail the build if the C side changes.

use core::ffi::{c_char, c_int, c_void};
use core::mem::{offset_of, size_of};

#[repr(C)]
pub struct VfsNode {
    pub name: [c_char; 256],
    pub is_dir: bool,
    pub has_perm: bool,
    pub perm: u16,
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

// typedef struct VFS_Mount {
//   char mountpoint[10];
//   VFS_FS *fs;
//   struct VFS_Mount *next;
// } VFS_Mount;
#[repr(C)]
pub struct VfsMount {
    pub mountpoint: [u8; 10],
    pub fs: *mut VfsFs,
    pub next: *mut VfsMount,
}

/* -- task_t (arch/x86/kernel/smp/task.h) ----------------------------------
 *
 * The scheduler's own task struct, mirrored field by field. Unlike
 * a small public struct it is internal: it changes whenever the scheduler does, and a
 * live task_t is only safe to read under its runqueue lock. The asserts
 * at the bottom catch layout changes; offsets came from gcc. */

/// task_state_t. Kept as a plain u32, not a Rust enum: a value C writes
/// that the enum doesn't list would be undefined behaviour.
pub const TASK_READY: u32 = 0;
pub const TASK_RUNNING: u32 = 1;
pub const TASK_BLOCKED: u32 = 2;
pub const TASK_SLEEPING: u32 = 3;
pub const TASK_ZOMBIE: u32 = 4;
pub const TASK_DEAD: u32 = 5;
pub const TASK_UNINTERRUPTIBLE: u32 = 6;

pub const MAX_FDS: usize = 64;

/// Types Rust only ever holds pointers to.
#[repr(C)]
pub struct VmMap {
    _opaque: [u8; 0],
}
#[repr(C)]
pub struct OpenFile {
    _opaque: [u8; 0],
}
#[repr(C)]
pub struct WaitQueue {
    _opaque: [u8; 0],
}
#[repr(C)]
pub struct SyscallAbi {
    _opaque: [u8; 0],
}

#[repr(C)]
pub struct FdEntry {
    pub file: *mut OpenFile, // NULL: free slot
    pub fd_flags: c_int,
}

/// cpu_context_t is __attribute__((packed)): fields may be unaligned,
/// so copy them out (`{ ctx.rip }`) instead of taking references.
#[repr(C, packed)]
pub struct CpuContext {
    pub r15: u64,
    pub r14: u64,
    pub r13: u64,
    pub r12: u64,
    pub r11: u64,
    pub r10: u64,
    pub r9: u64,
    pub r8: u64,
    pub rdi: u64,
    pub rsi: u64,
    pub rbp: u64,
    pub unused: u64,
    pub rbx: u64,
    pub rdx: u64,
    pub rcx: u64,
    pub rax: u64,
    pub rsp: u64,
    pub rip: u64,
    pub rflags: u64,
    pub cs: u64,
    pub ss: u64,
    pub ds: u64,
    pub es: u64,
    pub fs: u64,
    pub gs: u64,
    pub fpu_state: *mut c_void,
}

#[repr(C)]
pub struct TaskId {
    pub pid: u32,
    pub tid: u32,
    pub name: [u8; 32],
}

#[repr(C)]
pub struct TaskSched {
    pub priority: u32,
    pub time_slice: u64,
    pub time_slice_max: u64,
    pub total_runtime: u64,
    pub last_scheduled: u64,
    pub wake_at_ns: u64,
    pub cpu: u8,
    pub in_wait: bool,
}

#[repr(C)]
pub struct TaskExec {
    pub context: CpuContext,
    pub context_saved: bool,
    pub in_syscall: bool,
    pub in_syscall_rsp: u64,
    pub rsp0_size: u32,
    pub rsp0: u64,
}

#[repr(C)]
pub struct TaskMm {
    pub page_table: *mut u64,
    pub vm_map: *mut VmMap,
    pub heap_start: u64,
    pub heap_end: u64,
    pub fs_base: u64,
    pub tls_size: usize,
    pub gs_base: u64,
}

#[repr(C)]
pub struct TaskStacks {
    pub kernel_stack: u64,
    pub user_stack: u64,
    pub stack_size: usize,
    pub userspace: bool,
}

#[repr(C)]
pub struct TaskEntry {
    pub entry_point: Option<unsafe extern "C" fn(*mut c_void)>,
    pub entry_arg: *mut c_void,
}

#[repr(C)]
pub struct TaskFdTable {
    pub fds: [FdEntry; MAX_FDS],
    pub cwd: [u8; 256],
}

#[repr(C)]
pub struct TaskSignals {
    pub signal: u16,
    pub signal_handlers: *mut c_void,
}

#[repr(C)]
pub struct TaskLinkage {
    pub state: u32, // TASK_* above
    pub exit_code: c_int,
    pub lock: u16,
    pub spinlocks: u8,
    pub next: *mut Task,
    pub prev: *mut Task,
    pub parent: *mut Task,
    pub children: *mut Task,
    pub sibling: *mut Task,
}

#[repr(C)]
pub struct Task {
    pub id: TaskId,
    pub sched: TaskSched,
    pub exec: TaskExec,
    pub mm: TaskMm,
    pub stacks: TaskStacks,
    pub entry: TaskEntry,
    pub fdtable: TaskFdTable,
    pub signals: TaskSignals,
    pub linkage: TaskLinkage,
    pub child_wait: *mut WaitQueue,
    pub abi: *const SyscallAbi, // NULL for kernel threads
    pub pgid: u32,
    pub sid: u32,
    pub umask: u32,
}

const _: () = {
    assert!(size_of::<CpuContext>() == 208);
    assert!(size_of::<TaskSched>() == 56);
    assert!(offset_of!(TaskSched, cpu) == 48);
    assert!(size_of::<TaskExec>() == 240);
    assert!(offset_of!(TaskExec, rsp0_size) == 224);
    assert!(offset_of!(TaskExec, rsp0) == 232);
    assert!(size_of::<TaskMm>() == 56);
    assert!(offset_of!(TaskMm, fs_base) == 32);
    assert!(size_of::<TaskStacks>() == 32);
    assert!(size_of::<TaskFdTable>() == 1280);
    assert!(offset_of!(TaskFdTable, cwd) == 1024);
    assert!(size_of::<TaskLinkage>() == 56);
    assert!(offset_of!(TaskLinkage, spinlocks) == 10);
    assert!(offset_of!(TaskLinkage, next) == 16);
    assert!(offset_of!(TaskLinkage, sibling) == 48);
    assert!(size_of::<Task>() == 1824);
    assert!(offset_of!(Task, sched) == 40);
    assert!(offset_of!(Task, exec) == 96);
    assert!(offset_of!(Task, mm) == 336);
    assert!(offset_of!(Task, stacks) == 392);
    assert!(offset_of!(Task, entry) == 424);
    assert!(offset_of!(Task, fdtable) == 440);
    assert!(offset_of!(Task, signals) == 1720);
    assert!(offset_of!(Task, linkage) == 1736);
    assert!(offset_of!(Task, child_wait) == 1792);
    assert!(offset_of!(Task, abi) == 1800);
    assert!(offset_of!(Task, sid) == 1812);
};

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
    pub chmod: Unused,
    pub readlink: Unused,
}

// struct module {
//   char name[64];
//   char path[256];
//   uint64_t base;
//   size_t size;
//   void (*exit_fn)(void);
//   struct module *next;
//   module_section_info_t sections[MODULE_MAX_SECTIONS];
//   int section_count;
//   int export_count;
//   module_sym_export_t exports[MODULE_MAX_EXPORTS];
// };

// typedef struct {
//   char name[64];
//   uint64_t addr;
// } module_sym_export_t;

// #define MODULE_MAX_EXPORTS 512
// #define MODULE_MAX_SECTIONS 128

// /**
//  * @brief Section metadata stored per loaded module.
//  */
// typedef struct {
//   char name[64];
//   uint64_t base;
//   uint64_t size;
// } module_section_info_t;

#[repr(C)]
pub struct ModuleSymExport {
    pub name: [u8; 64],
    pub addr: u64,
}

#[repr(C)]
pub struct ModuleSectionInfo {
    pub name: [u8; 64],
    pub base: u64,
    pub size: u64,
}

#[repr(C)]
pub struct Module {
    pub name: [u8; 64],
    pub path: [u8; 256],
    pub base: u64,
    pub size: usize,
    pub exit_fn: Option<unsafe extern "C" fn()>,
    pub next: *mut Module,
    pub sections: [ModuleSectionInfo; 16],
    pub section_count: c_int,
    pub export_count: c_int,
    pub exports: [ModuleSymExport; 32],
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
    assert!(size_of::<VfsFs>() == 136);
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
    pub fn vfs_get_mounts() -> *mut VfsMount;
    pub fn task_snapshot(pids: *mut u32, max: usize) -> usize;
    pub fn task_get(pid: u32, out: *mut Task, ppid: *mut u32) -> bool;
    pub fn get_current_task() -> *mut Task;

    // int module_export_module_list(module_t **out_list, size_t *out_count)
    pub fn module_export_module_list(out_list: *mut *mut Module, out_count: *mut usize) -> c_int;
}
