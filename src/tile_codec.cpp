#include "corertt/tile_codec.h"

namespace cr {

std::uint8_t packTileFlags(const TileFlags &tile) noexcept {
	std::uint8_t packed = 0;
	packed |= static_cast<std::uint8_t>(tile.terrain & 0b11);
	packed |= static_cast<std::uint8_t>((tile.side & 0b11) << 2);
	packed |= static_cast<std::uint8_t>(tile.is_resource ? 0b00010000 : 0);
	packed |= static_cast<std::uint8_t>(tile.is_base ? 0b00100000 : 0);
	return packed;
}

TileFlags unpackTileFlags(std::uint8_t packed) noexcept {
	TileFlags tile;
	tile.terrain = packed & 0b11;
	tile.side = (packed >> 2) & 0b11;
	tile.is_resource = (packed & 0b00010000) != 0;
	tile.is_base = (packed & 0b00100000) != 0;
	return tile;
}

} // namespace cr
