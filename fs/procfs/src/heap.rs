//! Global allocator on top of kmalloc/kfree.
//!
//! kmalloc only guarantees 8-byte alignment (large allocations return
//! page + 8-byte header), so stricter alignments over-allocate and keep
//! the original pointer in the word just before the aligned block.

use core::alloc::{GlobalAlloc, Layout};
use core::ffi::c_void;

use crate::kernel::{GFP_KERNEL, kfree, kmalloc};

const NATIVE_ALIGN: usize = 8;

struct KernelHeap;

unsafe impl GlobalAlloc for KernelHeap {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        if layout.align() <= NATIVE_ALIGN {
            return unsafe { kmalloc(layout.size(), GFP_KERNEL) as *mut u8 };
        }

        let extra = layout.align() + size_of::<usize>();
        let raw = unsafe { kmalloc(layout.size() + extra, GFP_KERNEL) } as usize;
        if raw == 0 {
            return core::ptr::null_mut();
        }
        let aligned = (raw + size_of::<usize>() + layout.align() - 1) & !(layout.align() - 1);
        unsafe { *((aligned - size_of::<usize>()) as *mut usize) = raw };
        aligned as *mut u8
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        let raw = if layout.align() <= NATIVE_ALIGN {
            ptr as usize
        } else {
            unsafe { *((ptr as usize - size_of::<usize>()) as *const usize) }
        };
        unsafe { kfree(raw as *mut c_void) };
    }
}

#[global_allocator]
static HEAP: KernelHeap = KernelHeap;
