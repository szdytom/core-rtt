# rusty-corelib

A no_std-first Rust wrapper over Core RTT ecalls.

## Goals

- Rust-native API over raw ecalls.
- Cargo-first workflow.
- Default startup/runtime takeover for easy onboarding.
- Default allocator backed by runtime malloc/calloc/realloc/free.

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

## Usage

See [rusty-corelib API documentation](../docs/rusty-corelib.md) for details on how to use the library to write Rust guest programs.
