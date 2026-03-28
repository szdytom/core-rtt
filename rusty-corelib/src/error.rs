#[repr(i32)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum ErrorCode {
	InvalidPointer = -1,
	InvalidUnit = -2,
	InsufficientEnergy = -3,
	OnCooldown = -4,
	CapacityFull = -5,
	InvalidId = -6,
	OutOfRange = -7,
	UnsupportedRuntime = -8,
	Unknown = i32::MIN,
}

pub type Result<T> = core::result::Result<T, ErrorCode>;

impl ErrorCode {
	#[must_use]
	pub const fn from_raw(code: i32) -> Self {
		match code {
			-1 => Self::InvalidPointer,
			-2 => Self::InvalidUnit,
			-3 => Self::InsufficientEnergy,
			-4 => Self::OnCooldown,
			-5 => Self::CapacityFull,
			-6 => Self::InvalidId,
			-7 => Self::OutOfRange,
			-8 => Self::UnsupportedRuntime,
			_ => Self::Unknown,
		}
	}
}

#[must_use]
pub const fn status(code: i32) -> Result<()> {
	if code == 0 {
		Ok(())
	} else {
		Err(ErrorCode::from_raw(code))
	}
}

#[must_use]
pub const fn value(code: i32) -> Result<usize> {
	if code >= 0 {
		Ok(code as usize)
	} else {
		Err(ErrorCode::from_raw(code))
	}
}
