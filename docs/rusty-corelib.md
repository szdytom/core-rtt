# rusty-corelib Documentation

`rusty-corelib` is a `no_std` Rust support library for writing Core RTT guest
programs. It provides:

- Safe(ish) wrappers over runtime ecalls.
- Runtime entry/bootstrap (`_start`) via feature flags.
- Optional global allocator backed by runtime allocator ecalls.
- Rust-native types and enums for game state and commands.

This document is intended as the primary API reference.

## Integration

Add the crate as your guest runtime dependency and target the same RISC-V
configuration as Core RTT.

The repository includes a ready-to-use target config in
`rusty-corelib/.cargo/config.toml`:

- target: `riscv32imac-unknown-none-elf`
- linker: `rust-lld`
- linker script: `link.ld`
- target features: `+zba,+zbb,+zbc,+zbs`

Typical build commands:

```bash
rustup target add riscv32imac-unknown-none-elf
cargo build --release
cargo build --release --examples
```

## Runtime Model

By default (`runtime` feature enabled), the crate provides startup runtime code:

- `_start` symbol (entry point)
- `gp` initialization
- `.bss` zeroing
- `.init_array` execution
- panic handler that terminates through runtime `abort`

If you disable `runtime`, you are responsible for entry/bootstrap yourself.

### Entry Function Contract

User code should expose one non-returning function and register it via:

```rust
entry!(run);
```

The macro exports:

```rust
#[unsafe(no_mangle)]
pub extern "Rust" fn app_main() -> !
```

The runtime calls `app_main()` and never expects a return.

## Features

From `Cargo.toml`:

- `runtime` (default): startup + panic handler.
- `allocator` (default): installs `#[global_allocator]` using runtime
  `malloc/calloc/realloc/free` ecalls.
- `async` (default): enables `async_rt` helpers built on turn-driven polling.

Disable defaults when you need full control:

```bash
cargo build --no-default-features
```

## Async Utilities (`feature = "async"`)

Core RTT does not provide timer interrupts or OS event queues, but turn
advancement (`turn()`) is a natural async source. The `async_rt` module uses
that model and polls tasks once per turn.

Key APIs:

```rust
pub fn block_on<F: Future>(future: F) -> F::Output

pub struct Scheduler;
impl Scheduler {
	pub fn new() -> Self
	pub fn with_capacity(capacity: usize) -> Self
	pub fn spawn<F>(&mut self, future: F) -> TaskHandle
	where
		F: Future<Output = Result<()>> + 'static;
	pub fn tick(&mut self) -> Result<TaskResult>
	pub fn run_until_complete(&mut self) -> Result<()>
}
```

Example pattern:

```rust
use rusty_corelib::{Direction, Result, async_rt::move_unit};

pub async fn move_along_path(path: &[Direction]) -> Result<()> {
	for &dir in path {
		move_unit(dir).await?;
	}
	Ok(())
}
```

## Error Model

Most wrappers return:

```rust
type Result<T> = core::result::Result<T, ErrorCode>
```

Runtime return codes are mapped as:

- `0` => success for status-style ecalls.
- `>= 0` => success value for length/value-style ecalls.
- `< 0` => `ErrorCode`.

`ErrorCode` variants:

- `InvalidPointer`
- `InvalidUnit`
- `InsufficientEnergy`
- `OnCooldown`
- `CapacityFull`
- `InvalidId`
- `OutOfRange`
- `UnsupportedRuntime`
- `Unknown`

## API Surface

All public functions are re-exported at crate root (`pub use api::*`).

### Control and Runtime State

```rust
pub fn abort() -> !
pub fn turn() -> i32
pub fn rand_u32() -> u32
```

- `abort` never returns.
- `turn` and `rand_u32` are raw-value ecalls and do not use `Result`.

### Metadata and Device State

```rust
pub fn meta() -> Result<GameInfo>
pub fn dev_info() -> DeviceInfo
pub fn pos() -> PosInfo
pub fn meminfo() -> Result<MemoryInfo>
```

### Logging

```rust
pub fn log_bytes(bytes: &[u8]) -> Result<()>
pub fn log_fmt(args: core::fmt::Arguments<'_>) -> Result<()>
```

Convenience macros:

```rust
logf!("hp={} en={}", hp, en)?;
logln!("tick={}", rusty_corelib::turn())?;
```

Notes:

- `log_fmt` uses an internal fixed-size buffer with max payload 512 bytes.
- Formatting overflow returns `ErrorCode::OutOfRange`.
- Empty formatted payload is treated as no-op success.

### Sensor and Messaging

```rust
pub fn read_sensor_raw(out: &mut [u8]) -> Result<usize>
pub fn read_sensor(out: &mut [SensorTile]) -> Result<usize>
pub fn recv_msg(out: &mut [u8]) -> Result<usize>
pub fn send_msg(data: &[u8]) -> Result<()>
```

The returned `usize` is the number of valid elements/bytes written.

### Base Actions

```rust
pub fn manufact(id: u8) -> Result<()>
pub fn repair(id: u8) -> Result<()>
pub fn upgrade(id: u8, upgrade_type: UpgradeType) -> Result<()>
```

### Unit Actions

```rust
pub fn move_unit(direction: Direction) -> Result<()>
pub fn fire(direction: Direction, power: u16) -> Result<()>
pub fn deposit(amount: u16) -> Result<()>
pub fn withdraw(amount: u16) -> Result<()>
```

## Types and Constants

The crate re-exports all public types from `types`.

### Enums

```rust
pub enum Direction { Up, Right, Down, Left }
pub enum UpgradeType { Capacity, Vision, Damage }
pub enum Terrain { Empty, Water, Obstacle }
```

`Direction` helpers:

```rust
pub const fn x_offset(self) -> i8
pub const fn y_offset(self) -> i8
```

### Flags

`UpgradeFlags` is a `bitflags` type (`u8`):

- `CAPACITY`
- `VISION`
- `DAMAGE`

### Structs

```rust
#[repr(C)]
pub struct GameInfo {
	pub map_width: u8,
	pub map_height: u8,
	pub base_size: u8,
	pub unit_health: u8,
	pub natural_energy_rate: u8,
	pub resource_zone_energy_rate: u8,
	pub attack_cooldown: u8,
	pub capacity_lv1: u8,
	pub capacity_lv2: u8,
	pub capture_turn_threshold: u8,
	pub capacity_upgrade_cost: u16,
	pub vision_upgrade_cost: u16,
	pub damage_upgrade_cost: u16,
	pub manufact_cost: u16,
	pub reserved: [u8; 14],
}

#[repr(C)]
pub struct PosInfo {
	pub x: u8,
	pub y: u8,
	pub reserved: [u8; 2],
}

#[repr(C)]
pub struct MemoryInfo {
	pub bytes_free: u32,
	pub bytes_used: u32,
	pub chunks_used: u32,
}

pub struct DeviceInfo {
	pub id: u8,
	pub upgrades: UpgradeFlags,
	pub health: u8,
	pub energy: u16,
}

pub struct SensorTile { /* raw: u8 */ }
```

`DeviceInfo::from_raw(raw: u32)` decodes the packed runtime payload.

`SensorTile` exposes field inspectors:

- `visible()`
- `terrain()`
- `is_base()`
- `is_resource()`
- `has_unit()`
- `has_bullet()`
- `alliance_unit()`

and raw accessors:

- `SensorTile::new(raw)`
- `raw()`
- `decode_sensor_tile(raw)`

## Allocator Behavior

With `allocator` enabled on `target_os = "none"`, the crate installs a global
allocator implementation that maps to runtime allocator ecalls:

- alloc -> `malloc`
- alloc_zeroed -> `calloc`
- realloc -> `realloc`
- dealloc -> `free`

This makes heap-using `alloc` data structures possible in guest code, subject to
runtime memory limits.

## Safety and ABI Notes

- Low-level ecall ABI is implemented in `syscall.rs` using inline assembly and
  register convention (`a7` call id, args in `a0/a1`, return in `a0`).
- Non-RISC-V targets intentionally panic when calling ecall intrinsics.
- High-level API functions convert raw ABI to typed Rust values/results.

## Minimal Examples

### Continuous logging loop

```rust
#![no_std]
#![no_main]

use core::hint::spin_loop;
use rusty_corelib::{entry, logln};

fn run() -> ! {
	let _ = logln!("hello from rusty-corelib");
	loop {
		let _ = logln!("turn={}", rusty_corelib::turn());
		spin_loop();
	}
}

entry!(run);
```

### Unit action with error handling

```rust
use rusty_corelib::{Direction, ErrorCode};

fn try_move_right() {
	match rusty_corelib::move_unit(Direction::Right) {
		Ok(()) => {}
		Err(ErrorCode::OnCooldown) => {}
		Err(_) => {
			rusty_corelib::abort();
		}
	}
}
```

## Relationship with C corelib

`rusty-corelib` maps closely to C `corelib` concepts but favors Rust types,
`Result` returns, and enums/bitflags over integer constants.

- C `int` status codes -> Rust `Result<()>` / `Result<usize>`
- C enum constants -> Rust enums (`Direction`, `UpgradeType`, `Terrain`)
- C packed state payloads -> decoded Rust structs (`DeviceInfo`, `SensorTile`)

## Versioning

Current crate version: `0.1.0`.

Future additions should remain backward compatible when possible, and new ecalls
or wrappers should follow the same `Result` conversion conventions used in
`api.rs`.
