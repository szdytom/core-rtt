#include "corertt/cli.h"
#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace cr {

namespace {

void validateWorldRules(const GameRules &rules) {
	if (rules.width < 4 || rules.width > 255) {
		throw std::runtime_error("WIDTH must be within [4, 255]");
	}
	if (rules.height < 4 || rules.height > 255) {
		throw std::runtime_error("HEIGHT must be within [4, 255]");
	}
	if (rules.base_size < 2 || rules.base_size > 8) {
		throw std::runtime_error("BASE_SIZE must be within [2, 8]");
	}
	if (rules.unit_health < 1 || rules.unit_health > 255) {
		throw std::runtime_error("UNIT_HEALTH must be within [1, 255]");
	}
	if (rules.natural_energy_rate < 1 || rules.natural_energy_rate > 255) {
		throw std::runtime_error("NATURAL_ENERGY_RATE must be within [1, 255]");
	}
	if (rules.resource_zone_energy_rate < 1
	    || rules.resource_zone_energy_rate > 255) {
		throw std::runtime_error(
			"RESOURCE_ZONE_ENERGY_RATE must be within [1, 255]"
		);
	}
	if (rules.attack_cooldown < 1 || rules.attack_cooldown > 255) {
		throw std::runtime_error("ATTACK_COOLDOWN must be within [1, 255]");
	}
	if (rules.capture_turn_threshold < 1
	    || rules.capture_turn_threshold > 255) {
		throw std::runtime_error(
			"CAPTURE_TURN_THRESHOLD must be within [1, 255]"
		);
	}
	if (rules.vision_lv1 < 1 || rules.vision_lv1 > 255) {
		throw std::runtime_error("VISION_LV1 must be within [1, 255]");
	}
	if (rules.vision_lv2 < 1 || rules.vision_lv2 > 255) {
		throw std::runtime_error("VISION_LV2 must be within [1, 255]");
	}
	if (rules.capacity_lv1 < 1 || rules.capacity_lv1 > 65535) {
		throw std::runtime_error("CAPACITY_LV1 must be within [1, 65535]");
	}
	if (rules.capacity_lv2 < 1 || rules.capacity_lv2 > 65535) {
		throw std::runtime_error("CAPACITY_LV2 must be within [1, 65535]");
	}
	if (rules.capacity_upgrade_cost < 1
	    || rules.capacity_upgrade_cost > 65535) {
		throw std::runtime_error(
			"CAPACITY_UPGRADE_COST must be within [1, 65535]"
		);
	}
	if (rules.vision_upgrade_cost < 1 || rules.vision_upgrade_cost > 65535) {
		throw std::runtime_error(
			"VISION_UPGRADE_COST must be within [1, 65535]"
		);
	}
	if (rules.damage_upgrade_cost < 1 || rules.damage_upgrade_cost > 65535) {
		throw std::runtime_error(
			"DAMAGE_UPGRADE_COST must be within [1, 65535]"
		);
	}
	if (rules.manufact_cost < 1 || rules.manufact_cost > 65535) {
		throw std::runtime_error("MANUFACT_COST must be within [1, 65535]");
	}
	if (rules.capacity_lv2 < rules.capacity_lv1) {
		throw std::runtime_error("CAPACITY_LV2 must be >= CAPACITY_LV1");
	}
	if (rules.vision_lv2 < rules.vision_lv1) {
		throw std::runtime_error("VISION_LV2 must be >= VISION_LV1");
	}
}

} // namespace

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
	validateWorldRules(rules);

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
