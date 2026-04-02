#include "corertt/build_config.h"
#include "corertt/cli.h"
#include <argparse/argparse.hpp>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>

namespace cr {

namespace {

constexpr int FIXED_ZSTD_COMPRESSION_LEVEL = 12;

void validateWorldRuleParams(
	int width, int height, int base_size, int unit_health,
	std::uint32_t natural_energy_rate, int resource_zone_energy_rate,
	int attack_cooldown, int capture_turn_threshold, int vision_lv1,
	int vision_lv2, int capacity_lv1, int capacity_lv2,
	int capacity_upgrade_cost, int vision_upgrade_cost, int damage_upgrade_cost,
	int manufact_cost
) {}

UIMode parseUIMode(const std::string &value) {
	if (value == "tui") {
		return UIMode::Tui;
	}
	if (value == "plain") {
		return UIMode::Plain;
	}

	throw std::runtime_error(
		std::format("Invalid --ui-mode '{}', expected 'tui' or 'plain'", value)
	);
}

} // namespace

ProgramOptions parseOptions(
	int argc, char *argv[], const std::string &program_name, CliMode mode
) {
	argparse::ArgumentParser program(
		program_name, std::string(cr::program_version_with_commit)
	);

	program.add_argument("--width").default_value(64).scan<'i', int>().help(
		"tilemap width"
	);
	program.add_argument("--height")
		.default_value(64)
		.scan<'i', int>()
		.help("tilemap height");
	program.add_argument("--base-size")
		.default_value(5)
		.scan<'i', int>()
		.help("base side length");
	program.add_argument("--resources")
		.default_value(5)
		.scan<'i', int>()
		.help("number of resource clusters");
	program.add_argument("--unit-health")
		.default_value(100)
		.scan<'i', int>()
		.help("initial unit health");
	program.add_argument("--natural-energy-rate")
		.default_value(1u)
		.scan<'u', unsigned int>()
		.help("turns required for natural +1 unit energy");
	program.add_argument("--resource-zone-energy-rate")
		.default_value(25)
		.scan<'i', int>()
		.help("resource zone bonus energy per turn");
	program.add_argument("--attack-cooldown")
		.default_value(3)
		.scan<'i', int>()
		.help("turn cooldown after unit attack");
	program.add_argument("--capacity-lv1")
		.default_value(200)
		.scan<'i', int>()
		.help("unit capacity before capacity upgrade");
	program.add_argument("--capacity-lv2")
		.default_value(1000)
		.scan<'i', int>()
		.help("unit capacity after capacity upgrade");
	program.add_argument("--vision-lv1")
		.default_value(5)
		.scan<'i', int>()
		.help("unit vision range before vision upgrade");
	program.add_argument("--vision-lv2")
		.default_value(9)
		.scan<'i', int>()
		.help("unit vision range after vision upgrade");
	program.add_argument("--capacity-upgrade-cost")
		.default_value(400)
		.scan<'i', int>()
		.help("energy cost of capacity upgrade");
	program.add_argument("--vision-upgrade-cost")
		.default_value(1000)
		.scan<'i', int>()
		.help("energy cost of vision upgrade");
	program.add_argument("--damage-upgrade-cost")
		.default_value(600)
		.scan<'i', int>()
		.help("energy cost of damage upgrade");
	program.add_argument("--manufact-cost")
		.default_value(500)
		.scan<'i', int>()
		.help("energy cost for base manufacturing");
	program.add_argument("--capture-turn-threshold")
		.default_value(8)
		.scan<'i', int>()
		.help("consecutive capture turns required to lose");
	program.add_argument("--p1-base")
		.default_value(std::string("player1_base.elf"))
		.help("path to player 1 base ELF");
	program.add_argument("--p1-unit")
		.default_value(std::string("player1_unit.elf"))
		.help("path to player 1 unit ELF");
	program.add_argument("--p2-base")
		.default_value(std::string("player2_base.elf"))
		.help("path to player 2 base ELF");
	program.add_argument("--p2-unit")
		.default_value(std::string("player2_unit.elf"))
		.help("path to player 2 unit ELF");
	program.add_argument("--map")
		.default_value(std::string(""))
		.help("path to tilemap file (text or binary)");
	program.add_argument("-s", "--seed")
		.help(
			"seed for map generation: either any string, or 32 hex chars "
			"(omitted means device-random)"
		);
	program.add_argument("--replay-file")
		.default_value(std::string(""))
		.help("optional path to write binary replay file to");
	program.add_argument("-z", "--output-zstd")
		.flag()
		.help("compress replay output with zstd (fixed compression level: 12)");
	program.add_argument("--max-ticks")
		.default_value(0u)
		.scan<'u', unsigned int>()
		.help(
			"maximum number of ticks to simulate before auto draw "
			"(0=unlimited)"
		);
	program.add_argument("--dynamic-draw")
		.flag()
		.help("use the rule-recommended dynamic draw limit");

	if (mode == CliMode::Headless) {
		program.add_argument("--worker-mode")
			.flag()
			.help("don't enable this unless you know what it does");
	} else if (mode == CliMode::Interactive) {
		program.add_argument("--step-interval-ms")
			.default_value(200)
			.scan<'i', int>()
			.help("step or playback interval in milliseconds");
		program.add_argument("--ui-mode")
			.default_value(std::string("tui"))
			.help("UI mode: tui or plain");
		program.add_argument("--play-replay")
			.default_value(std::string(""))
			.help("optional replay file path for playback mode");
	}

	try {
		program.parse_args(argc, argv);
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		std::cerr << program;
		throw;
	}

	ProgramOptions options;
	options.width = program.get<int>("--width");
	options.height = program.get<int>("--height");
	options.base_size = program.get<int>("--base-size");
	options.resources = program.get<int>("--resources");
	options.p1_base = program.get<std::string>("--p1-base");
	options.p1_unit = program.get<std::string>("--p1-unit");
	options.p2_base = program.get<std::string>("--p2-base");
	options.p2_unit = program.get<std::string>("--p2-unit");
	options.map_file = program.get<std::string>("--map");
	if (program.is_used("--seed")) {
		options.seed = Seed::fromString(program.get<std::string>("--seed"));
	}
	options.replay_file = program.get<std::string>("--replay-file");
	options.max_ticks = program.get<unsigned int>("--max-ticks");

	if (mode == CliMode::Headless) {
		options.worker_mode = program.get<bool>("--worker-mode");
	}

	if (mode == CliMode::Interactive) {
		options.step_interval_ms = program.get<int>("--step-interval-ms");
		options.play_replay = program.get<std::string>("--play-replay");
		options.ui_mode = parseUIMode(program.get<std::string>("--ui-mode"));
	}

	options.dynamic_draw = program.get<bool>("--dynamic-draw");
	if (options.dynamic_draw && options.max_ticks > 0) {
		throw std::runtime_error(
			"--dynamic-draw conflicts with --max-ticks; specify at most one"
		);
	}

	options.output_zstd = program.get<bool>("--output-zstd");
	options.output_zstd_level = FIXED_ZSTD_COMPRESSION_LEVEL;

	const int unit_health = program.get<int>("--unit-health");
	const std::uint32_t natural_energy_rate = program.get<unsigned int>(
		"--natural-energy-rate"
	);
	const int resource_zone_energy_rate = program.get<int>(
		"--resource-zone-energy-rate"
	);
	const int attack_cooldown = program.get<int>("--attack-cooldown");
	const int capacity_lv1 = program.get<int>("--capacity-lv1");
	const int capacity_lv2 = program.get<int>("--capacity-lv2");
	const int vision_lv1 = program.get<int>("--vision-lv1");
	const int vision_lv2 = program.get<int>("--vision-lv2");
	const int capacity_upgrade_cost = program.get<int>(
		"--capacity-upgrade-cost"
	);
	const int vision_upgrade_cost = program.get<int>("--vision-upgrade-cost");
	const int damage_upgrade_cost = program.get<int>("--damage-upgrade-cost");
	const int manufact_cost = program.get<int>("--manufact-cost");
	const int capture_turn_threshold = program.get<int>(
		"--capture-turn-threshold"
	);

	const bool used_random_generation_option = program.is_used("--width")
		|| program.is_used("--height") || program.is_used("--base-size")
		|| program.is_used("--resources") || program.is_used("--seed");

	if (mode == CliMode::Interactive && !options.map_file.empty()
	    && !options.play_replay.empty()) {
		throw std::runtime_error("--map cannot be used with --play-replay");
	}

	if (mode == CliMode::Interactive && used_random_generation_option
	    && !options.play_replay.empty()) {
		throw std::runtime_error(
			"Random generation options cannot be used with --play-replay"
		);
	}

	if (mode == CliMode::Interactive && !options.replay_file.empty()
	    && !options.play_replay.empty()) {
		throw std::runtime_error(
			"--replay-file cannot be used with --play-replay"
		);
	}

	if (mode == CliMode::Interactive && options.output_zstd
	    && options.replay_file.empty()) {
		throw std::runtime_error(
			"--output-zstd requires --replay-file in interactive mode"
		);
	}

	if (mode == CliMode::Headless && options.worker_mode
	    && !options.output_zstd) {
		throw std::runtime_error(
			"--worker-mode requires --output-zstd in headless mode"
		);
	}

	if (!options.map_file.empty() && used_random_generation_option) {
		throw std::runtime_error(
			"--map cannot be combined with random generation options: "
			"--width/--height/--base-size/--resources/--seed"
		);
	}

	if (options.map_file.empty() && !options.seed.has_value()) {
		options.seed = Seed::deviceRandom();
	}

	if (used_random_generation_option) {
		if (options.width < 4 || options.width > 255) {
			throw std::runtime_error("WIDTH must be within [4, 255]");
		}
		if (options.height < 4 || options.height > 255) {
			throw std::runtime_error("HEIGHT must be within [4, 255]");
		}
		if (options.base_size < 2 || options.base_size > 8) {
			throw std::runtime_error("BASE_SIZE must be within [2, 8]");
		}
	}

	if (unit_health < 1 || unit_health > 255) {
		throw std::runtime_error("UNIT_HEALTH must be within [1, 255]");
	}
	if (natural_energy_rate < 1 || natural_energy_rate > 255) {
		throw std::runtime_error("NATURAL_ENERGY_RATE must be within [1, 255]");
	}
	if (resource_zone_energy_rate < 1 || resource_zone_energy_rate > 255) {
		throw std::runtime_error(
			"RESOURCE_ZONE_ENERGY_RATE must be within [1, 255]"
		);
	}
	if (attack_cooldown < 1 || attack_cooldown > 255) {
		throw std::runtime_error("ATTACK_COOLDOWN must be within [1, 255]");
	}
	if (capture_turn_threshold < 1 || capture_turn_threshold > 255) {
		throw std::runtime_error(
			"CAPTURE_TURN_THRESHOLD must be within [1, 255]"
		);
	}
	if (vision_lv1 < 1 || vision_lv1 > 255) {
		throw std::runtime_error("VISION_LV1 must be within [1, 255]");
	}
	if (vision_lv2 < 1 || vision_lv2 > 255) {
		throw std::runtime_error("VISION_LV2 must be within [1, 255]");
	}
	if (capacity_lv1 < 1 || capacity_lv1 > 65535) {
		throw std::runtime_error("CAPACITY_LV1 must be within [1, 65535]");
	}
	if (capacity_lv2 < 1 || capacity_lv2 > 65535) {
		throw std::runtime_error("CAPACITY_LV2 must be within [1, 65535]");
	}
	if (capacity_upgrade_cost < 1 || capacity_upgrade_cost > 65535) {
		throw std::runtime_error(
			"CAPACITY_UPGRADE_COST must be within [1, 65535]"
		);
	}
	if (vision_upgrade_cost < 1 || vision_upgrade_cost > 65535) {
		throw std::runtime_error(
			"VISION_UPGRADE_COST must be within [1, 65535]"
		);
	}
	if (damage_upgrade_cost < 1 || damage_upgrade_cost > 65535) {
		throw std::runtime_error(
			"DAMAGE_UPGRADE_COST must be within [1, 65535]"
		);
	}
	if (manufact_cost < 1 || manufact_cost > 65535) {
		throw std::runtime_error("MANUFACT_COST must be within [1, 65535]");
	}
	if (capacity_lv2 < capacity_lv1) {
		throw std::runtime_error("CAPACITY_LV2 must be >= CAPACITY_LV1");
	}
	if (vision_lv2 < vision_lv1) {
		throw std::runtime_error("VISION_LV2 must be >= VISION_LV1");
	}

	options.rules.unit_health = unit_health;
	options.rules.natural_energy_rate = natural_energy_rate;
	options.rules.resource_zone_energy_rate = resource_zone_energy_rate;
	options.rules.attack_cooldown = attack_cooldown;
	options.rules.capacity_lv1 = capacity_lv1;
	options.rules.capacity_lv2 = capacity_lv2;
	options.rules.vision_lv1 = vision_lv1;
	options.rules.vision_lv2 = vision_lv2;
	options.rules.capacity_upgrade_cost = capacity_upgrade_cost;
	options.rules.vision_upgrade_cost = vision_upgrade_cost;
	options.rules.damage_upgrade_cost = damage_upgrade_cost;
	options.rules.manufact_cost = manufact_cost;
	options.rules.capture_turn_threshold = capture_turn_threshold;

	return options;
}

} // namespace cr
