use bitflags::bitflags;

#[repr(i32)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Direction {
	Up = 0,
	Right = 1,
	Down = 2,
	Left = 3,
}

impl Direction {
	/// Returns the x-axis delta for one tile step in this direction.
	#[must_use]
	pub const fn x_offset(self) -> i8 {
		match self {
			Self::Right => 1,
			Self::Left => -1,
			_ => 0,
		}
	}

	/// Returns the y-axis delta for one tile step in this direction.
	#[must_use]
	pub const fn y_offset(self) -> i8 {
		match self {
			Self::Down => 1,
			Self::Up => -1,
			_ => 0,
		}
	}
}

#[repr(i32)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum UpgradeType {
	Capacity = 0,
	Vision = 1,
	Damage = 2,
}

#[repr(u8)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum Terrain {
	Empty = 0,
	Water = 1,
	Obstacle = 3,
}

bitflags! {
	#[derive(Copy, Clone, Debug, Eq, PartialEq)]
	pub struct UpgradeFlags: u8 {
		const CAPACITY = 0b001;
		const VISION = 0b010;
		const DAMAGE = 0b100;
	}
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
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
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub struct PosInfo {
	pub x: u8,
	pub y: u8,
	pub reserved: [u8; 2],
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub struct MemoryInfo {
	pub bytes_free: u32,
	pub bytes_used: u32,
	pub chunks_used: u32,
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub struct DeviceInfo {
	pub id: u8,
	pub upgrades: UpgradeFlags,
	pub health: u8,
	pub energy: u16,
}

impl DeviceInfo {
	/// Decodes a packed runtime payload into a structured [`DeviceInfo`].
	#[must_use]
	pub const fn from_raw(raw: u32) -> Self {
		let b0 = (raw & 0xFF) as u8;
		let b1 = ((raw >> 8) & 0xFF) as u8;
		let b2 = ((raw >> 16) & 0xFF) as u8;
		Self {
			id: b0 & 0b1_1111,
			upgrades: UpgradeFlags::from_bits_retain((b0 >> 5) & 0b111),
			health: b1,
			energy: b2 as u16 | (((raw >> 24) as u16) << 8),
		}
	}
}

#[repr(transparent)]
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub struct SensorTile {
	raw: u8,
}

impl SensorTile {
	/// Returns an all-zero tile value useful as a placeholder.
	pub const fn uninitialized() -> Self {
		Self { raw: 0 }
	}

	/// Creates a tile from the raw packed byte returned by sensor ecalls.
	pub const fn new(raw: u8) -> Self {
		Self { raw }
	}

	/// Returns the underlying packed tile byte.
	#[must_use]
	pub const fn raw(self) -> u8 {
		self.raw
	}

	/// Returns whether this tile is visible to the current device.
	#[must_use]
	pub const fn visible(self) -> bool {
		(self.raw & 0b0000_0001) != 0
	}

	/// Returns the terrain kind, or `None` for reserved bit patterns.
	#[must_use]
	pub const fn terrain(self) -> Option<Terrain> {
		match (self.raw >> 1) & 0b11 {
			0 => Some(Terrain::Empty),
			1 => Some(Terrain::Water),
			3 => Some(Terrain::Obstacle),
			_ => None,
		}
	}

	/// Returns whether this tile belongs to a base area.
	#[must_use]
	pub const fn is_base(self) -> bool {
		(self.raw & 0b0000_1000) != 0
	}

	/// Returns whether this tile is part of a resource zone.
	#[must_use]
	pub const fn is_resource(self) -> bool {
		(self.raw & 0b0001_0000) != 0
	}

	/// Returns whether there is a unit on this tile.
	#[must_use]
	pub const fn has_unit(self) -> bool {
		(self.raw & 0b0010_0000) != 0
	}

	/// Returns whether there is a bullet on this tile.
	#[must_use]
	pub const fn has_bullet(self) -> bool {
		(self.raw & 0b0100_0000) != 0
	}

	/// Returns whether the unit on this tile is allied.
	///
	/// This is meaningful only when [`Self::has_unit`] is `true`.
	#[must_use]
	pub const fn alliance_unit(self) -> bool {
		(self.raw & 0b1000_0000) != 0
	}
}

/// Decodes a raw packed sensor byte into a [`SensorTile`].
#[must_use]
pub const fn decode_sensor_tile(raw: u8) -> SensorTile {
	SensorTile::new(raw)
}
