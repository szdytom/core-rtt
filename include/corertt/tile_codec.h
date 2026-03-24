#ifndef CORERTT_TILE_CODEC_H
#define CORERTT_TILE_CODEC_H

#include <cstdint>

namespace cr {

struct TileFlags {
	std::uint8_t terrain = 0;
	std::uint8_t side = 0;
	bool is_resource = false;
	bool is_base = false;
};

std::uint8_t packTileFlags(const TileFlags &tile) noexcept;
TileFlags unpackTileFlags(std::uint8_t packed) noexcept;

} // namespace cr

#endif
