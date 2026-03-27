# rusty-corelib

A no_std-first Rust wrapper over Core RTT ecalls.

## Goals

- Rust-native API over raw ecalls.
- Cargo-first workflow.
- Default startup/runtime takeover for easy onboarding.
- Default allocator backed by runtime malloc/calloc/realloc/free.

## What is included in v0.1

- Core state and utility calls: `log`, `meta`, `turn`, `dev_info`, `pos`, `rand_u32`, `meminfo`.
- Sensor and messaging: `read_sensor`, `send_msg`, `recv_msg`.
- Base actions: `manufact`, `repair`, `upgrade`.
- Unit actions: `move_unit`, `fire`, `deposit`, `withdraw`.
- Runtime startup: `_start` + `.bss` clearing + `init_array` execution + `panic_handler`.
- Macros: `entry!`, `logf!`, `logln!`.

## Build

Install target once:

```bash
rustup target add riscv32imac-unknown-none-elf
```

Build library:

```bash
cargo build --release
```

Build examples:

```bash
cargo build --release --example base
cargo build --release --example unit
```

## Usage pattern

```rust
#![no_std]
#![no_main]

use rusty_corelib::{entry, logln};

fn run() -> ! {
    loop {
        let t = rusty_corelib::turn();
        let _ = logln!("turn={}", t);
    }
}

entry!(run);
```

## Features

- `runtime` (default): exposes startup runtime and panic handler.
- `allocator` (default): installs global allocator backed by runtime ecalls.

Disable defaults for advanced setups:

```bash
cargo build --no-default-features
```

## no_std ecosystem notes

These crates are often useful and work in no_std contexts (usually with default features disabled):

- `bitflags` for compact flag types.
- `heapless` for fixed-capacity collections without a heap.
- `arrayvec` for stack-backed vectors/strings.
- `fixed` for fixed-point arithmetic.
- `micromath` for small-footprint math helpers.

Always verify binary size and instruction budget impact in your bot.
