#include "corertt/tilemap.h"
#include "corertt/buffer.h"
#include "corertt/noise.h"
#include "corertt/stream_adapter.h"
#include "corertt/tile_codec.h"
#include "corertt/xoroshiro.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <iterator>
#include <limits>
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
constexpr std::uint8_t TILEMAP_BINARY_VERSION = 2;

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

Tilemap Tilemap::load(std::istream &input_stream) {
	if (!input_stream || input_stream.eof()) {
		throw std::runtime_error("Input stream is not valid for reading");
	}

	int first_char = input_stream.peek();
	if (!std::isdigit(first_char)) {
		return loadBinary(input_stream);
	}
	return loadText(input_stream);
}

Tilemap Tilemap::loadBinary(std::istream &input_stream) {
	if (!input_stream || input_stream.eof()) {
		throw std::runtime_error("Input stream is not valid for reading");
	}

	IstreamAdapter stream_adapter(input_stream);
	ReadBuffer reader(stream_adapter);
	constexpr std::size_t header_prefix_size = tilemap_binary_magic.size() + 1
		+ 4 * sizeof(std::uint16_t);

	if (!reader.has(header_prefix_size)) {
		throw std::runtime_error("Binary tilemap data is too short");
	}

	for (const auto expected : tilemap_binary_magic) {
		if (reader.readU8() != expected) {
			throw std::runtime_error("Invalid binary tilemap magic");
		}
	}

	const auto version = reader.readU8();
	if (version != TILEMAP_BINARY_VERSION) {
		throw std::runtime_error(
			std::format("Unsupported binary tilemap version {}", version)
		);
	}

	const pos_t width = reader.readU16();
	const pos_t height = reader.readU16();
	const pos_t base_size = reader.readU16();
	reader.skip(2);

	constexpr pos_t MAX_DIMENSION = 512;
	if (width <= 0 || height <= 0 || width > MAX_DIMENSION
	    || height > MAX_DIMENSION) {
		throw std::runtime_error(
			std::format("Invalid tilemap size: {} x {}", width, height)
		);
	}

	const std::size_t tile_count = width * height;
	if (!reader.has(tile_count)) {
		throw std::runtime_error(
			std::format(
				"Binary tilemap size mismatch: expected at least {} bytes",
				tile_count
			)
		);
	}

	Tilemap tilemap(width, height);
	tilemap._base_size = base_size;

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const auto flags = TileFlags::unpack(reader.readU8());
			const auto terrain = flags.terrain;
			const auto side = flags.side;
			const auto is_resource = flags.is_resource;
			const auto is_base = flags.is_base;

			if (terrain != Tile::EMPTY && terrain != Tile::WATER
			    && terrain != Tile::OBSTACLE) {
				throw std::runtime_error(
					std::format(
						"Invalid terrain code {} at ({}, {})", terrain, x, y
					)
				);
			}

			if (side > 2) {
				throw std::runtime_error(
					std::format("Invalid side code {} at ({}, {})", side, x, y)
				);
			}

			if (!is_base && side != 0) {
				throw std::runtime_error(
					std::format(
						"Non-base tile has nonzero side {} at ({}, {})", side,
						x, y
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

	// Check base is square and consistent with base_size
	bool base_found[2] = {false, false};
	int base_x[2] = {0, 0};
	int base_y[2] = {0, 0};
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const Tile &tile = tilemap.tileOf(x, y);
			if (!tile.is_base) {
				continue;
			}

			const auto side = tile.side;
			if (base_found[tile.side - 1]) {
				if (x < base_x[side - 1] || x >= base_x[side - 1] + base_size
				    || y < base_y[side - 1]
				    || y >= base_y[side - 1] + base_size) {
					throw std::runtime_error(
						std::format("Base for player {} is invalid", tile.side)
					);
				}
				continue; // Already validated this base area
			}

			base_found[side - 1] = true;
			base_x[side - 1] = x;
			base_y[side - 1] = y;

			if (x + base_size > width || y + base_size > height) {
				throw std::runtime_error(
					std::format(
						"Base for player {} exceeds map bounds", tile.side
					)
				);
			}

			for (int dy = 0; dy < base_size; ++dy) {
				for (int dx = 0; dx < base_size; ++dx) {
					const int nx = x + dx;
					const int ny = y + dy;

					const Tile &base_tile = tilemap.tileOf(nx, ny);
					if (!base_tile.is_base || base_tile.side != side) {
						throw std::runtime_error(
							std::format(
								"Base at ({}, {}) is not a square of size {}",
								x, y, base_size
							)
						);
					}
				}
			}
		}
	}

	if (!reader.end()) {
		throw std::runtime_error(
			"Binary tilemap size mismatch: trailing bytes found"
		);
	}

	return tilemap;
}

Tilemap Tilemap::loadText(std::istream &input_stream) {
	if (!input_stream || input_stream.eof()) {
		throw std::runtime_error("Input stream is not valid for reading");
	}

	const std::string text_data{
		std::istreambuf_iterator<char>(input_stream),
		std::istreambuf_iterator<char>()
	};
	if (text_data.empty()) {
		throw std::runtime_error("Cannot load tilemap from empty data");
	}

	// Text format: first line is width and height, then rows of terrain chars
	std::istringstream iss(text_data);
	int width, height;
	iss >> width >> height;
	iss.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	Tilemap tilemap(width, height);
	// #: obstacle, .: empty, ~: water, $: resource(empty), B: base(empty)
	std::string line;
	for (int y = 0; y < height; ++y) {
		std::getline(iss, line);
		// Filter out whitespace
		line.erase(
			std::remove_if(line.begin(), line.end(), ::isspace), line.end()
		);
		if (line.size() != static_cast<size_t>(width)) {
			throw std::runtime_error(
				std::format(
					"Line {} has incorrect width: expected {}, got {}", y + 2,
					width, line.size()
				)
			);
		}

		for (int x = 0; x < width; ++x) {
			Tile &tile = tilemap.tileOf(x, y);
			switch (line[x]) {
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
						"Invalid terrain character '{}' at ({}, {})", line[x],
						x, y
					)
				);
			}
		}
	}

	// Ensure there are 2 bases and both bases are squares of same size
	// Assign sides for bases

	int common_base_size = 0;
	int base_found = 0;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			Tile &tile = tilemap.tileOf(x, y);
			if (!tile.is_base || tile.side != 0) {
				continue;
			}

			base_found += 1;
			if (base_found > 2) {
				throw std::runtime_error(
					std::format(
						"More than 2 bases found, extra base at ({}, {})", x, y
					)
				);
			}

			// Found a base tile, determine its size by scanning right (+x)
			int base_size = 1;
			while (x + base_size < width
			       && tilemap.tileOf(x + base_size, y).is_base) {
				base_size++;
			}

			if (common_base_size == 0) {
				common_base_size = base_size;
			} else if (base_size != common_base_size) {
				throw std::runtime_error(
					std::format(
						"Base at ({}, {}) has different size {} than previous "
						"base size {}",
						x, y, base_size, common_base_size
					)
				);
			}

			// Mark the entire base area and assign side
			for (int bx = x; bx < x + base_size; ++bx) {
				for (int by = y; by < y + base_size; ++by) {
					Tile &base_tile = tilemap.tileOf(bx, by);
					if (!base_tile.is_base) {
						throw std::runtime_error(
							std::format(
								"Base at ({}, {}) is not a square of size {}",
								x, y, base_size
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
	os.put(static_cast<char>(TILEMAP_BINARY_VERSION));

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

	WriteBuffer payload;
	payload.writeU16(_width);
	payload.writeU16(_height);
	payload.writeU16(_base_size);
	payload.writeU16(0);

	for (int y = 0; y < _height; ++y) {
		for (int x = 0; x < _width; ++x) {
			const Tile &tile = tileOf(x, y);
			TileFlags flags{
				.terrain = tile.terrain,
				.side = tile.side,
				.is_resource = tile.is_resource,
				.is_base = tile.is_base,
			};
			payload.writeU8(flags.pack());
		}
	}

	const auto &bytes = payload.bytes();
	os.write(
		reinterpret_cast<const char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
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
	UniformPerlinNoise noise(rng.jump96());
	noise.calibrate(
		config.noise_scale, config.noise_octaves, config.noise_persistence
	);

	constexpr double obstacle_threshold = 0.8;
	constexpr double water_threshold = 0.2;

	Tilemap tilemap(config.width, config.height);
	for (int y = 0; y < config.height; ++y) {
		for (int x = 0; x < config.width; ++x) {
			double value = noise.uniform_noise(y, x);
			Tile &tile = tilemap.tileOf(x, y);
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
	const int base_x[] = {0, config.width - config.base_size};
	const int base_y[] = {0, config.height - config.base_size};
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
