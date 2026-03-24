#ifndef CORERTT_TILEMAP_H
#define CORERTT_TILEMAP_H

#include "corertt/entity.h"
#include "corertt/xoroshiro.h"
#include <cstdint>
#include <iosfwd>
#include <istream>
#include <memory>
#include <utility>

#ifndef NDEBUG
#include <cpptrace/cpptrace.hpp>
#include <cstdlib>
#include <iostream>
#endif

namespace cr {

struct TilemapGenerationConfig {
	Seed seed;
	pos_t width = 32;
	pos_t height = 32;

	double noise_scale = 0.15;
	int noise_octaves = 3;
	double noise_persistence = 0.4;

	pos_t base_size = 5; // n means n x n square base
	int num_resources = 5;
};

struct Tile {
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

	union {
		Unit *unit_ptr;     // non-owned pointer to unit on this tile
		Bullet *bullet_ptr; // non-owned pointer to bullet on this tile
		std::nullptr_t null_ptr;
	};

	void unsetOccupant() noexcept;
};

class Tilemap {
public:
	Tilemap(pos_t width, pos_t height);

	pos_t width() const noexcept {
		return _width;
	}

	pos_t height() const noexcept {
		return _height;
	}

	pos_t baseSize() const noexcept {
		return _base_size;
	}

	Tile tileOf(int x, int y) const noexcept {
#ifndef NDEBUG
		if (x < 0 || x >= _width || y < 0 || y >= _height) {
			std::cerr << std::format(
				"Tilemap::tileOf() out of bounds access: ({}, {}) with size "
				"({}, {})\n",
				x, y, _width, _height
			);
			cpptrace::generate_trace().print(std::cerr);
			std::abort();
		}
#endif
		return _tiles[y * _width + x];
	}

	Tile &tileOf(int x, int y) noexcept {
#ifndef NDEBUG
		if (x < 0 || x >= _width || y < 0 || y >= _height) {
			std::cerr << std::format(
				"Tilemap::tileOf() out of bounds access: ({}, {}) with size "
				"({}, {})\n",
				x, y, _width, _height
			);
			cpptrace::generate_trace().print(std::cerr);
			std::abort();
		}
#endif
		return _tiles[y * _width + x];
	}

	static Tilemap generate(const TilemapGenerationConfig &config);
	static Tilemap load(std::istream &input_stream);
	static Tilemap loadBinary(std::istream &input_stream);
	static Tilemap loadText(std::istream &input_stream);
	void saveAsText(std::ostream &os) const;
	void saveAsBinary(std::ostream &os) const;

private:
	pos_t _width;
	pos_t _height;
	pos_t _base_size;
	std::unique_ptr<Tile[]> _tiles;
};

} // namespace cr

#endif
