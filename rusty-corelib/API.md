# rusty-corelib API v0.1

This is the high-level Rust API map.

## State and environment

- `turn() -> i32`
- `meta() -> Result<GameInfo>`
- `dev_info() -> DeviceInfo`
- `pos() -> PosInfo`
- `rand_u32() -> u32`
- `meminfo() -> Result<MemoryInfo>`

## Logging

- `log_bytes(&[u8]) -> Result<()>`
- `log_fmt(core::fmt::Arguments) -> Result<()>`
- `logf!` and `logln!`

## Sensor and communication

- `read_sensor_raw(&mut [u8]) -> Result<usize>`
- `read_sensor(&mut [SensorTile]) -> Result<usize>`
- `send_msg(&[u8]) -> Result<()>`
- `recv_msg(&mut [u8]) -> Result<usize>`

## Base runtime actions

- `manufact(id: u8) -> Result<()>`
- `repair(id: u8) -> Result<()>`
- `upgrade(id: u8, upgrade_type: UpgradeType) -> Result<()>`

## Unit runtime actions

- `move_unit(direction: Direction) -> Result<()>`
- `fire(direction: Direction, power: u16) -> Result<()>`
- `deposit(amount: u16) -> Result<()>`
- `withdraw(amount: u16) -> Result<()>`

## Startup and entry

- Default feature `runtime` provides `_start` and panic handler.
- Declare your logic function and bind it with `entry!(run_fn)`.

## Error model

- `Result<T> = core::result::Result<T, ErrorCode>`
- Runtime negative codes are mapped to `ErrorCode`.
