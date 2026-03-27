#[cfg(all(feature = "allocator", target_os = "none"))]
use core::alloc::{GlobalAlloc, Layout};

#[cfg(all(feature = "allocator", target_os = "none"))]
struct EcallAllocator;

#[cfg(all(feature = "allocator", target_os = "none"))]
unsafe impl GlobalAlloc for EcallAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        // SAFETY: Runtime allocator validates size and returns a managed pointer.
        unsafe { crate::syscall::malloc(layout.size()) }
    }

    unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
        // SAFETY: Runtime allocator validates arguments and returns a managed pointer.
        unsafe { crate::syscall::calloc(1, layout.size()) }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        // SAFETY: Runtime allocator accepts null and owned pointers.
        unsafe { crate::syscall::free(ptr) };
    }

    unsafe fn realloc(&self, ptr: *mut u8, _layout: Layout, new_size: usize) -> *mut u8 {
        // SAFETY: Runtime allocator validates arguments and returns a managed pointer.
        unsafe { crate::syscall::realloc(ptr, new_size) }
    }
}

#[cfg(all(feature = "allocator", target_os = "none"))]
#[global_allocator]
static GLOBAL_ALLOCATOR: EcallAllocator = EcallAllocator;
