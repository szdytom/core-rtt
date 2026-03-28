use core::fmt;

use crate::error::{ErrorCode, Result, status, value};
use crate::syscall;
use crate::types::{
	DeviceInfo, Direction, GameInfo, MemoryInfo, PosInfo, SensorTile,
	UpgradeType,
};

#[must_use]
pub fn turn() -> i32 {
	// SAFETY: Zero-arg ecall wrapper.
	unsafe { syscall::turn() }
}

pub fn abort() -> ! {
	// SAFETY: Runtime contract says abort does not return.
	unsafe { syscall::abort() }
}

pub fn log_bytes(bytes: &[u8]) -> Result<()> {
	// SAFETY: Slice pointer/length come from Rust references.
	let code = unsafe { syscall::log(bytes.as_ptr(), bytes.len()) };
	status(code)
}

const LOG_MAX_BYTES: usize = 512;

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

#[must_use]
pub fn dev_info() -> DeviceInfo {
	// SAFETY: Raw payload is decoded in pure Rust.
	let raw = unsafe { syscall::dev_info_raw() };
	DeviceInfo::from_raw(raw)
}

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

pub fn read_sensor_raw(out: &mut [u8]) -> Result<usize> {
	// SAFETY: Slice pointer/length come from Rust references.
	let code = unsafe { syscall::read_sensor(out.as_mut_ptr()) };
	value(code)
}

pub fn read_sensor(out: &mut [SensorTile]) -> Result<usize> {
	let raw =
        // SAFETY: SensorTile is a single-byte newtype, so the cast preserves layout.
        unsafe { core::slice::from_raw_parts_mut(out.as_mut_ptr().cast::<u8>(), out.len()) };
	read_sensor_raw(raw)
}

pub fn send_msg(data: &[u8]) -> Result<()> {
	// SAFETY: Slice pointer/length come from Rust references.
	let code = unsafe { syscall::send_msg(data.as_ptr(), data.len()) };
	status(code)
}

pub fn recv_msg(out: &mut [u8]) -> Result<usize> {
	// SAFETY: Slice pointer/length come from Rust references.
	let code = unsafe { syscall::recv_msg(out.as_mut_ptr(), out.len()) };
	value(code)
}

pub fn manufact(id: u8) -> Result<()> {
	// SAFETY: Runtime validates id and runtime role.
	let code = unsafe { syscall::manufact(id as i32) };
	status(code)
}

pub fn repair(id: u8) -> Result<()> {
	// SAFETY: Runtime validates id and runtime role.
	let code = unsafe { syscall::repair(id as i32) };
	status(code)
}

pub fn upgrade(id: u8, upgrade_type: UpgradeType) -> Result<()> {
	// SAFETY: Runtime validates arguments and runtime role.
	let code = unsafe { syscall::upgrade(id as i32, upgrade_type as i32) };
	status(code)
}

pub fn fire(direction: Direction, power: u16) -> Result<()> {
	// SAFETY: Runtime validates arguments and runtime role.
	let code = unsafe { syscall::fire(direction as i32, i32::from(power)) };
	status(code)
}

pub fn move_unit(direction: Direction) -> Result<()> {
	// SAFETY: Runtime validates direction and runtime role.
	let code = unsafe { syscall::move_unit(direction as i32) };
	status(code)
}

pub fn deposit(amount: u16) -> Result<()> {
	// SAFETY: Runtime validates amount and runtime role.
	let code = unsafe { syscall::deposit(i32::from(amount)) };
	status(code)
}

pub fn withdraw(amount: u16) -> Result<()> {
	// SAFETY: Runtime validates amount and runtime role.
	let code = unsafe { syscall::withdraw(i32::from(amount)) };
	status(code)
}

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

pub fn log_fmt(args: fmt::Arguments<'_>) -> Result<()> {
	let mut buffer = LogBuffer::new();
	fmt::write(&mut buffer, args).map_err(|_| ErrorCode::OutOfRange)?;
	if buffer.is_empty() {
		return Ok(());
	}

	log_bytes(buffer.as_slice())
}
