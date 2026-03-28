use core::fmt;

use crate::error::{ErrorCode, Result, status, value};
use crate::syscall;
use crate::types::{
	DeviceInfo, Direction, GameInfo, MemoryInfo, PosInfo, SensorTile,
	UpgradeType,
};

/// Returns the current game turn number.
///
/// The first turn is `1` and increments by one each tick.
#[must_use]
pub fn turn() -> i32 {
	// SAFETY: Zero-arg ecall wrapper.
	unsafe { syscall::turn() }
}

/// Aborts the current runtime instance immediately.
///
/// This call never returns.
pub fn abort() -> ! {
	// SAFETY: Runtime contract says abort does not return.
	unsafe { syscall::abort() }
}

/// Writes a raw byte slice into the game log.
///
/// Returns an error when the runtime rejects the payload (for example, invalid
/// length, invalid pointer, or exhausted log quota).
pub fn log_bytes(bytes: &[u8]) -> Result<()> {
	// SAFETY: Slice pointer/length come from Rust references.
	let code = unsafe { syscall::log(bytes.as_ptr(), bytes.len()) };
	status(code)
}

const LOG_MAX_BYTES: usize = 512;

/// Fetches immutable game metadata.
///
/// This includes map width/height and base size for the current match.
pub fn meta() -> Result<GameInfo> {
	let mut info = GameInfo {
		map_width: 0,
		map_height: 0,
		base_size: 0,
		reserved: [0; 13],
	};
	// SAFETY: `info` is a valid writable pointer.
	let code = unsafe { syscall::meta((&mut info as *mut GameInfo).cast()) };
	status(code)?;
	Ok(info)
}

/// Returns the current device state (base or unit).
#[must_use]
pub fn dev_info() -> DeviceInfo {
	// SAFETY: Raw payload is decoded in pure Rust.
	let raw = unsafe { syscall::dev_info_raw() };
	DeviceInfo::from_raw(raw)
}

/// Returns the current device position.
///
/// For base runtime, this is the top-left tile of the base area.
#[must_use]
pub fn pos() -> PosInfo {
	// SAFETY: The payload layout is a packed 32-bit PosInfo.
	let raw = unsafe { syscall::pos_raw() };
	PosInfo {
		x: (raw & 0xFF) as u8,
		y: ((raw >> 8) & 0xFF) as u8,
		reserved: [((raw >> 16) & 0xFF) as u8, ((raw >> 24) & 0xFF) as u8],
	}
}

/// Reads raw sensor bytes into `out`.
///
/// On success, returns the number of bytes written by the runtime.
pub fn read_sensor_raw(out: &mut [u8]) -> Result<usize> {
	// SAFETY: Slice pointer/length come from Rust references.
	let code = unsafe { syscall::read_sensor(out.as_mut_ptr()) };
	value(code)
}

/// Reads sensor tiles into `out`.
///
/// On success, returns the number of tiles written by the runtime.
pub fn read_sensor(out: &mut [SensorTile]) -> Result<usize> {
	let raw =
        // SAFETY: SensorTile is a single-byte newtype, so the cast preserves layout.
        unsafe { core::slice::from_raw_parts_mut(out.as_mut_ptr().cast::<u8>(), out.len()) };
	read_sensor_raw(raw)
}

/// Broadcasts a message to all allied devices.
///
/// The payload length must follow runtime limits (`1..=512`).
pub fn send_msg(data: &[u8]) -> Result<()> {
	// SAFETY: Slice pointer/length come from Rust references.
	let code = unsafe { syscall::send_msg(data.as_ptr(), data.len()) };
	status(code)
}

/// Receives one message from the incoming queue.
///
/// On success, returns the number of bytes copied into `out`.
pub fn recv_msg(out: &mut [u8]) -> Result<usize> {
	// SAFETY: Slice pointer/length come from Rust references.
	let code = unsafe { syscall::recv_msg(out.as_mut_ptr(), out.len()) };
	value(code)
}

/// Manufactures a unit with the provided unit `id`.
///
/// Available in base runtime only.
pub fn manufact(id: u8) -> Result<()> {
	// SAFETY: Runtime validates id and runtime role.
	let code = unsafe { syscall::manufact(id as i32) };
	status(code)
}

/// Repairs a unit with the provided unit `id`.
///
/// Available in base runtime only.
pub fn repair(id: u8) -> Result<()> {
	// SAFETY: Runtime validates id and runtime role.
	let code = unsafe { syscall::repair(id as i32) };
	status(code)
}

/// Applies an upgrade to the unit `id`.
///
/// Available in base runtime only.
pub fn upgrade(id: u8, upgrade_type: UpgradeType) -> Result<()> {
	// SAFETY: Runtime validates arguments and runtime role.
	let code = unsafe { syscall::upgrade(id as i32, upgrade_type as i32) };
	status(code)
}

/// Fires a bullet in `direction` with the given `power`.
///
/// Available in unit runtime only.
pub fn fire(direction: Direction, power: u16) -> Result<()> {
	// SAFETY: Runtime validates arguments and runtime role.
	let code = unsafe { syscall::fire(direction as i32, i32::from(power)) };
	status(code)
}

/// Moves the current unit by one tile in `direction`.
///
/// Available in unit runtime only.
pub fn move_unit(direction: Direction) -> Result<()> {
	// SAFETY: Runtime validates direction and runtime role.
	let code = unsafe { syscall::move_unit(direction as i32) };
	status(code)
}

/// Deposits up to `amount` energy from the current unit into the base.
///
/// Available in unit runtime only.
pub fn deposit(amount: u16) -> Result<()> {
	// SAFETY: Runtime validates amount and runtime role.
	let code = unsafe { syscall::deposit(i32::from(amount)) };
	status(code)
}

/// Withdraws up to `amount` energy from the base into the current unit.
///
/// Available in unit runtime only.
pub fn withdraw(amount: u16) -> Result<()> {
	// SAFETY: Runtime validates amount and runtime role.
	let code = unsafe { syscall::withdraw(i32::from(amount)) };
	status(code)
}

/// Returns sandbox allocator usage statistics.
pub fn meminfo() -> Result<MemoryInfo> {
	let mut info = MemoryInfo {
		bytes_free: 0,
		bytes_used: 0,
		chunks_used: 0,
	};
	// SAFETY: `info` is a valid writable pointer.
	let code =
		unsafe { syscall::meminfo((&mut info as *mut MemoryInfo).cast()) };
	status(code)?;
	Ok(info)
}

/// Returns a runtime-provided pseudo-random `u32` value.
#[must_use]
pub fn rand_u32() -> u32 {
	// SAFETY: Zero-arg ecall wrapper.
	unsafe { syscall::rand_u32() }
}

struct LogBuffer {
	bytes: [u8; LOG_MAX_BYTES],
	len: usize,
}

impl LogBuffer {
	fn new() -> Self {
		Self {
			bytes: [0; LOG_MAX_BYTES],
			len: 0,
		}
	}

	fn as_slice(&self) -> &[u8] {
		&self.bytes[..self.len]
	}

	fn is_empty(&self) -> bool {
		self.len == 0
	}
}

impl fmt::Write for LogBuffer {
	fn write_str(&mut self, s: &str) -> fmt::Result {
		if s.is_empty() {
			return Ok(());
		}

		let chunk = s.as_bytes();
		let next_len = self.len + chunk.len();
		if next_len > LOG_MAX_BYTES {
			return Err(fmt::Error);
		}

		self.bytes[self.len..next_len].copy_from_slice(chunk);
		self.len = next_len;

		Ok(())
	}
}

/// Formats log content into an internal 512-byte buffer and submits it.
///
/// Returns `ErrorCode::OutOfRange` when the formatted payload would exceed the
/// runtime single-call log limit.
pub fn log_fmt(args: fmt::Arguments<'_>) -> Result<()> {
	let mut buffer = LogBuffer::new();
	fmt::write(&mut buffer, args).map_err(|_| ErrorCode::OutOfRange)?;
	if buffer.is_empty() {
		return Ok(());
	}

	log_bytes(buffer.as_slice())
}
