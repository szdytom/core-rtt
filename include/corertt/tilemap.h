#ifndef CORERTT_TILEMAP_H
#define CORERTT_TILEMAP_H

#include "corertt/xoroshiro.h"
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <span>
#include <utility>

#ifndef NDEBUG
#include <cpptrace/cpptrace.hpp>
#include <cstdlib>
#include <iostream>
#endif

namespace cr {

struct TilemapGenerationConfig {
	Seed seed;
	int width = 32;
	int height = 32;

	double noise_scale = 0.15;
	int noise_octaves = 3;
	double noise_persistence = 0.4;

	int base_size = 5; // n means n x n square base
	int num_resources = 5;
};

struct alignas(int) Tile {
	static constexpr int EMPTY = 0;
	static constexpr int WATER = 1;
	static constexpr int OBSTACLE = 3;

	static constexpr int NOT_OCCUPIED = 0;
	static constexpr int OCCUPIED_BY_UNIT = 1;
	static constexpr int OCCUPIED_BY_BULLET = 2;

	Tile() noexcept;

	std::uint8_t terrain : 2;
	std::uint8_t side : 2; // 0: none, 1/2: player 1/2
	bool is_resource : 1;
	bool is_base : 1;
	std::uint8_t occupied_state : 2;
	std::uint16_t entity_id;

	bool operator==(this Tile self, Tile other) noexcept = default;
};

static_assert(sizeof(Tile) == 4, "Tile size should be 4 bytes");

class Tilemap {
public:
	Tilemap(int width, int height);

	int width() const noexcept {
		return _width;
	}

	int height() const noexcept {
		return _height;
	}

	template<typename Self>
	auto &&tileOf(this Self &&self, int x, int y) noexcept {
#ifndef NDEBUG
		if (x < 0 || x >= self._width || y < 0 || y >= self._height) {
			std::cerr << std::format(
				"Tilemap::tileOf() out of bounds access: ({}, {}) with size "
				"({}, {})\n",
				x, y, self._width, self._height
			);
			cpptrace::generate_trace().print(std::cerr);
			std::abort();
		}
#endif
		return std::forward<Self>(self)._tiles[y * self._width + x];
	}

	static Tilemap generate(const TilemapGenerationConfig &config);
	static Tilemap load(std::span<char> data);
	void saveAsText(std::ostream &os) const;

private:
	int _width;
	int _height;
	std::unique_ptr<Tile[]> _tiles;
};

} // namespace cr

#endif
