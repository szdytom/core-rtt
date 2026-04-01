#include "corertt/cli.h"
#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace cr {

std::vector<std::uint8_t> loadBinary(const std::string &path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		throw std::runtime_error(
			std::format("Failed to open ELF file: {}", path)
		);
	}

	auto content = std::vector<std::uint8_t>(
		std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()
	);

	if (content.size() < 4 || content[0] != 0x7F || content[1] != 'E'
	    || content[2] != 'L' || content[3] != 'F') {
		throw std::runtime_error(
			std::format("File is not a valid ELF binary: {}", path)
		);
	}

	return content;
}

Tilemap loadTilemap(const std::string &path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		throw std::runtime_error(
			std::format("Failed to open tilemap file: {}", path)
		);
	}

	return Tilemap::load(file);
}

Tilemap generateTilemap(const ProgramOptions &options) {
	TilemapGenerationConfig config;
	config.width = options.width;
	config.height = options.height;
	config.base_size = options.base_size;
	config.num_resources = options.resources;
	if (options.seed.has_value()) {
		config.seed = *options.seed;
	} else {
		config.seed = Seed::deviceRandom();
	}

	return Tilemap::generate(config);
}

World createWorldFromOptions(const ProgramOptions &options) {
	Tilemap tilemap = options.map_file.empty()
		? generateTilemap(options)
		: loadTilemap(options.map_file);
	GameRules rules = options.rules;
	rules.width = tilemap.width();
	rules.height = tilemap.height();
	rules.base_size = tilemap.baseSize();

	World world(std::move(tilemap), rules);
	world.setPlayerProgram(
		1, loadBinary(options.p1_base), loadBinary(options.p1_unit)
	);
	world.setPlayerProgram(
		2, loadBinary(options.p2_base), loadBinary(options.p2_unit)
	);
	return world;
}

} // namespace cr
