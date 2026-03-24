#include "corertt/tile_codec.h"

namespace cr {

std::uint8_t TileFlags::pack(this TileFlags tile) noexcept {
	std::uint8_t packed = 0;
	packed |= static_cast<std::uint8_t>(tile.terrain & 0b11);
	packed |= static_cast<std::uint8_t>((tile.side & 0b11) << 2);
	packed |= static_cast<std::uint8_t>(tile.is_resource ? 0b00010000 : 0);
	packed |= static_cast<std::uint8_t>(tile.is_base ? 0b00100000 : 0);
	return packed;
}

TileFlags TileFlags::unpack(std::uint8_t packed) noexcept {
	return {
		.terrain = static_cast<std::uint8_t>(packed & 0b11),
		.side = static_cast<std::uint8_t>((packed >> 2) & 0b11),
		.is_resource = (packed & 0b00010000) != 0,
		.is_base = (packed & 0b00100000) != 0,
	};
}

} // namespace cr
