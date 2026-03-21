#include "corertt/tilemap.h"
#include "corertt/noise.h"
#include "corertt/xoroshiro.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <ostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

namespace cr {

namespace {

constexpr std::array<std::uint8_t, 6> tilemap_binary_magic = {
	'C', 'R', 'T', 'M', 'A', 'P'
};
constexpr std::uint8_t tilemap_binary_version = 1;

void writeU16Le(std::ostream &os, std::uint16_t value) {
	if constexpr (std::endian::native == std::endian::big) {
		value = std::byteswap(value);
	}
	os.write(reinterpret_cast<const char *>(&value), sizeof(value));
}

std::uint16_t readU16Le(std::span<const char> data, std::size_t &offset) {
	if (offset + sizeof(std::uint16_t) > data.size()) {
		throw std::runtime_error("Unexpected end of binary tilemap data");
	}

	std::uint16_t value = 0;
	std::memcpy(&value, data.data() + offset, sizeof(value));
	if constexpr (std::endian::native == std::endian::big) {
		value = std::byteswap(value);
	}
	offset += sizeof(value);
	return value;
}

} // namespace

Tile::Tile() noexcept
	: terrain(EMPTY)
	, side(0)
	, is_resource(false)
	, is_base(false)
	, occupied_state(NOT_OCCUPIED)
	, null_ptr(nullptr) {}

void Tile::unsetOccupant() noexcept {
	null_ptr = nullptr;
	occupied_state = NOT_OCCUPIED;
}

Tilemap::Tilemap(pos_t width, pos_t height)
	: _width(width)
	, _height(height)
	, _tiles(std::make_unique<Tile[]>(width * height)) {}

Tilemap Tilemap::load(std::span<char> data) {
	if (data.empty()) {
		throw std::runtime_error("Cannot load tilemap from empty data");
	}

	if (!std::isdigit(data[0])) {
		std::span<const char> binary_data(data.data(), data.size());

		if (binary_data.size()
		    < tilemap_binary_magic.size() + 1 + 3 * sizeof(std::uint16_t)) {
			throw std::runtime_error("Binary tilemap data is too short");
		}

		if (!std::equal(
				binary_data.begin(),
				binary_data.begin() + tilemap_binary_magic.size(),
				tilemap_binary_magic.begin()
			)) {
			throw std::runtime_error("Invalid binary tilemap magic");
		}

		std::size_t offset = tilemap_binary_magic.size();
		const auto version = static_cast<std::uint8_t>(binary_data[offset]);
		offset += 1;
		if (version != tilemap_binary_version) {
			throw std::runtime_error(
				std::format("Unsupported binary tilemap version {}", version)
			);
		}

		const auto width = static_cast<pos_t>(readU16Le(binary_data, offset));
		const auto height = static_cast<pos_t>(readU16Le(binary_data, offset));
		const auto base_size = static_cast<pos_t>(
			readU16Le(binary_data, offset)
		);

		if (width <= 0 || height <= 0) {
			throw std::runtime_error(
				std::format("Invalid tilemap size: {} x {}", width, height)
			);
		}

		const std::size_t tile_count = static_cast<std::size_t>(width)
			* static_cast<std::size_t>(height);
		const std::size_t expected_size = offset + tile_count;
		if (binary_data.size() != expected_size) {
			throw std::runtime_error(
				std::format(
					"Binary tilemap size mismatch: expected {} bytes, got {}",
					expected_size, binary_data.size()
				)
			);
		}

		Tilemap tilemap(width, height);
		tilemap._base_size = base_size;

		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				const auto packed = static_cast<std::uint8_t>(
					binary_data[offset++]
				);

				const auto terrain = packed & 0b0000'0011;
				const auto side = (packed >> 2) & 0b0000'0011;
				const auto is_resource = (packed & 0b0001'0000) != 0;
				const auto is_base = (packed & 0b0010'0000) != 0;

				if (terrain == 0b0000'0010) {
					throw std::runtime_error(
						std::format(
							"Invalid terrain code {} at ({}, {})", terrain, x, y
						)
					);
				}

				if (side > 2) {
					throw std::runtime_error(
						std::format(
							"Invalid side code {} at ({}, {})", side, x, y
						)
					);
				}

				Tile &tile = tilemap.tileOf(x, y);
				tile.terrain = terrain;
				tile.side = side;
				tile.is_resource = is_resource;
				tile.is_base = is_base;
				tile.unsetOccupant();
			}
		}

		return tilemap;
	}

	// Text format: first line is width and height, then rows of terrain chars
	std::istringstream iss(std::string(data.data(), data.size()));
	int width, height;
	iss >> width >> height;
	Tilemap tilemap(width, height);
	// #: obstacle, .: empty, ~: water, R: resource(empty), B: base(empty)
	std::string line;
	for (int i = 0; i < height; ++i) {
		std::getline(iss, line);
		// Filter out whitespace
		line.erase(
			std::remove_if(line.begin(), line.end(), ::isspace), line.end()
		);
		if (line.size() != static_cast<size_t>(width)) {
			throw std::runtime_error(
				std::format(
					"Line {} has incorrect width: expected {}, got {}", i + 2,
					width, line.size()
				)
			);
		}

		for (int j = 0; j < width; ++j) {
			Tile &tile = tilemap.tileOf(i, j);
			switch (line[j]) {
			case '#':
				tile.terrain = Tile::OBSTACLE;
				break;
			case '.':
				tile.terrain = Tile::EMPTY;
				break;
			case '~':
				tile.terrain = Tile::WATER;
				break;
			case '$':
				tile.terrain = Tile::EMPTY;
				tile.is_resource = true;
				break;
			case 'B':
				tile.terrain = Tile::EMPTY;
				tile.is_base = true;
				break;
			default:
				throw std::runtime_error(
					std::format(
						"Invalid terrain character '{}' at ({}, {})", line[j],
						i, j
					)
				);
			}
		}
	}

	// Ensure there are 2 bases and both bases are squares of same size
	// Assign sides for bases

	int common_base_size = 0;
	int base_found = 0;
	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			Tile &tile = tilemap.tileOf(i, j);
			if (!tile.is_base || tile.side != 0) {
				continue;
			}

			base_found += 1;
			if (base_found > 2) {
				throw std::runtime_error(
					std::format(
						"More than 2 bases found, extra base at ({}, {})", i, j
					)
				);
			}

			// Found a base tile, determine its size
			int base_size = 1;
			while (j + base_size < width
			       && tilemap.tileOf(i, j + base_size).is_base) {
				base_size++;
			}

			if (common_base_size == 0) {
				common_base_size = base_size;
			} else if (base_size != common_base_size) {
				throw std::runtime_error(
					std::format(
						"Base at ({}, {}) has different size {} than previous "
						"base size {}",
						i, j, base_size, common_base_size
					)
				);
			}

			// Mark the entire base area and assign side
			for (int x = i; x < i + base_size; ++x) {
				for (int y = j; y < j + base_size; ++y) {
					Tile &base_tile = tilemap.tileOf(x, y);
					if (!base_tile.is_base) {
						throw std::runtime_error(
							std::format(
								"Base at ({}, {}) is not a square of size {}",
								i, j, base_size
							)
						);
					}
					base_tile.side = base_found; // Player 1 or 2
				}
			}
		}
	}

	tilemap._base_size = common_base_size;

	return tilemap;
}

void Tilemap::saveAsText(std::ostream &os) const {
	os << _width << " " << _height << "\n";
	for (int y = 0; y < _height; ++y) {
		for (int x = 0; x < _width; ++x) {
			const Tile &tile = tileOf(x, y);
			if (tile.is_base) {
				os << 'B';
			} else if (tile.is_resource) {
				os << '$';
			} else {
				switch (tile.terrain) {
				case Tile::EMPTY:
					os << '.';
					break;
				case Tile::WATER:
					os << '~';
					break;
				case Tile::OBSTACLE:
					os << '#';
					break;
				default:
					throw std::runtime_error(
						std::format(
							"Invalid terrain type {} at ({}, {})", tile.terrain,
							x, y
						)
					);
				}
			}
			os << " \n"[x == _width - 1];
		}
	}
}

void Tilemap::saveAsBinary(std::ostream &os) const {
	os.write(
		reinterpret_cast<const char *>(tilemap_binary_magic.data()),
		tilemap_binary_magic.size()
	);
	os.put(static_cast<char>(tilemap_binary_version));

	if (_width <= 0 || _height <= 0) {
		throw std::runtime_error(
			std::format("Invalid tilemap size: {} x {}", _width, _height)
		);
	}

	if (_base_size < 0) {
		throw std::runtime_error(
			std::format("Invalid base size: {}", _base_size)
		);
	}

	writeU16Le(os, static_cast<std::uint16_t>(_width));
	writeU16Le(os, static_cast<std::uint16_t>(_height));
	writeU16Le(os, static_cast<std::uint16_t>(_base_size));

	for (int y = 0; y < _height; ++y) {
		for (int x = 0; x < _width; ++x) {
			const Tile &tile = tileOf(x, y);
			std::uint8_t packed = 0;
			packed |= tile.terrain & 0b0000'0011;
			packed |= (tile.side & 0b0000'0011) << 2;
			if (tile.is_resource) {
				packed |= 0b0001'0000;
			}
			if (tile.is_base) {
				packed |= 0b0010'0000;
			}
			os.put(static_cast<char>(packed));
		}
	}
}

Tilemap Tilemap::generate(const TilemapGenerationConfig &config) {
	constexpr int min_map_size = 16;
	constexpr int max_map_size = 1000;
	constexpr int max_base_size = min_map_size / 2 - 1;

	if (config.width < min_map_size || config.height < min_map_size) {
		throw std::runtime_error(
			std::format(
				"Tilemap size too small: width {} height {}", config.width,
				config.height
			)
		);
	}

	if (config.width > max_map_size || config.height > max_map_size) {
		throw std::runtime_error(
			std::format(
				"Tilemap size too large: width {} height {}", config.width,
				config.height
			)
		);
	}

	if (config.base_size <= 0 || config.base_size > max_base_size) {
		throw std::runtime_error(
			std::format(
				"Invalid base size {}: must be between 1 and {}",
				config.base_size, max_base_size
			)
		);
	}

	Xoroshiro128PP rng(config.seed);
	UniformPerlinNoise noise(rng.jump_96());
	noise.calibrate(
		config.noise_scale, config.noise_octaves, config.noise_persistence
	);

	constexpr double obstacle_threshold = 0.8;
	constexpr double water_threshold = 0.2;

	Tilemap tilemap(config.width, config.height);
	for (int i = 0; i < config.height; ++i) {
		for (int j = 0; j < config.width; ++j) {
			double value = noise.uniform_noise(i, j);
			Tile &tile = tilemap.tileOf(i, j);
			if (value > obstacle_threshold) {
				tile.terrain = Tile::OBSTACLE;
			} else if (value < water_threshold) {
				tile.terrain = Tile::WATER;
			} else {
				tile.terrain = Tile::EMPTY;
			}
		}
	}

	// TODO: Check connectivity
	// Remove small holes and connect larger holes using MST

	// Place bases. For simplicity, we place them in the corners, for now.
	const int base_x[] = {0, config.height - config.base_size};
	const int base_y[] = {0, config.width - config.base_size};
	tilemap._base_size = config.base_size;
	for (int i = 0; i < 2; ++i) {
		int x = base_x[i];
		int y = base_y[i];
		for (int dx = 0; dx < config.base_size; ++dx) {
			for (int dy = 0; dy < config.base_size; ++dy) {
				Tile &base_tile = tilemap.tileOf(x + dx, y + dy);
				base_tile.is_base = true;
				base_tile.side = i + 1;          // Player 1 or 2
				base_tile.terrain = Tile::EMPTY; // Ensure base is on empty
				                                 // terrain
			}
		}
	}

	// Place resources
	std::uniform_int_distribution<int> x_dist(0, config.width - 1);
	std::uniform_int_distribution<int> y_dist(0, config.height - 1);
	int resources_placed = 0;
	constexpr int resource_pattern_size = 2; // Place resources in 2x2 clusters
	for (int attempt = 0; attempt < config.num_resources * 10
	     && resources_placed < config.num_resources;
	     ++attempt) {
		int x = x_dist(rng);
		int y = y_dist(rng);
		Tile &tile = tilemap.tileOf(x, y);
		if (x + resource_pattern_size > config.width
		    || y + resource_pattern_size > config.height) {
			continue; // Skip if pattern would go out of bounds
		}

		// Check if the 2x2 area is suitable for placing a resource, i.e. all
		// tiles are empty and not already occupied by a base

		for (int dx = 0; dx < resource_pattern_size; ++dx) {
			for (int dy = 0; dy < resource_pattern_size; ++dy) {
				Tile &check_tile = tilemap.tileOf(x + dx, y + dy);
				if (check_tile.terrain != Tile::EMPTY || check_tile.is_base) {
					goto next_attempt; // Not suitable, try another position
				}
			}
		}

		// Place the resource in the 2x2 area
		for (int dx = 0; dx < resource_pattern_size; ++dx) {
			for (int dy = 0; dy < resource_pattern_size; ++dy) {
				Tile &resource_tile = tilemap.tileOf(x + dx, y + dy);
				resource_tile.is_resource = true;
			}
		}
		resources_placed += 1;

	next_attempt:
	}

	return tilemap;
}

} // namespace cr
