#![no_std]

#[cfg(feature = "allocator")]
extern crate alloc;

pub mod api;
pub mod error;
pub mod syscall;
pub mod types;

#[cfg(feature = "runtime")]
pub mod runtime;

#[cfg(feature = "allocator")]
mod alloc_impl;

pub use api::*;
pub use error::{ErrorCode, Result};
pub use types::*;

#[macro_export]
macro_rules! logf {
    ($($arg:tt)*) => {{
        $crate::api::log_fmt(core::format_args!($($arg)*))
    }};
}

#[macro_export]
macro_rules! logln {
    () => {{
        $crate::api::log_fmt(core::format_args!("\n"))
    }};
    ($($arg:tt)*) => {{
        $crate::api::log_fmt(core::format_args!($($arg)*))
    }};
}

#[macro_export]
macro_rules! entry {
    ($path:path) => {
        #[unsafe(no_mangle)]
        pub extern "Rust" fn app_main() -> ! {
            $path()
        }
    };
}
