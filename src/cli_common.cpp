#include "corertt/cli_common.h"
#include "corertt/build_config.h"
#include <argparse/argparse.hpp>
#include <format>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace cr {

namespace {

UiMode parseUiMode(const std::string &value) {
	if (value == "tui") {
		return UiMode::Tui;
	}
	if (value == "plain") {
		return UiMode::Plain;
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
	program.add_argument("--step-interval-ms")
		.default_value(200)
		.scan<'i', int>()
		.help("step or playback interval in milliseconds");
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
		.help("seed for map generation (omitted means device-random)");
	if (mode == CliMode::Headless) {
		program.add_argument("--replay-file")
			.required()
			.help("path to write binary replay in headless mode");
		program.add_argument("--max-ticks")
			.default_value(0u)
			.scan<'u', unsigned int>()
			.help(
				"maximum number of ticks to simulate before auto draw "
				"(0=unlimited)"
			);
	} else {
		program.add_argument("--ui-mode")
			.default_value(std::string("tui"))
			.help("UI mode: tui or plain");
		program.add_argument("--replay-file")
			.default_value(std::string(""))
			.help("optional path to write binary replay in live mode");
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
	options.step_interval_ms = program.get<int>("--step-interval-ms");
	options.p1_base = program.get<std::string>("--p1-base");
	options.p1_unit = program.get<std::string>("--p1-unit");
	options.p2_base = program.get<std::string>("--p2-base");
	options.p2_unit = program.get<std::string>("--p2-unit");
	options.map_file = program.get<std::string>("--map");
	if (program.is_used("--seed")) {
		options.seed = program.get<std::string>("--seed");
	}
	options.replay_file = program.get<std::string>("--replay-file");
	if (mode == CliMode::Headless) {
		options.max_ticks = program.get<unsigned int>("--max-ticks");
	}
	try {
		if (mode == CliMode::Interactive) {
			options.play_replay = program.get<std::string>("--play-replay");
			options.ui_mode = parseUiMode(
				program.get<std::string>("--ui-mode")
			);
		}

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

		if (!options.map_file.empty() && used_random_generation_option) {
			throw std::runtime_error(
				"--map cannot be combined with random generation options: "
				"--width/--height/--base-size/--resources/--seed"
			);
		}
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		std::cerr << program;
		throw;
	}

	return options;
}

std::vector<std::uint8_t> loadBinary(std::string_view path) {
	std::ifstream file(path.data(), std::ios::binary);
	if (!file) {
		throw std::runtime_error(
			std::format("Failed to open ELF file: {}", path)
		);
	}

	auto content = std::vector<std::uint8_t>(
		std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()
	);

	// Quick check for ELF magic number (0x7F 'E' 'L' 'F')
	if (content.size() < 4 || content[0] != 0x7F || content[1] != 'E'
	    || content[2] != 'L' || content[3] != 'F') {
		throw std::runtime_error(
			std::format("File is not a valid ELF binary: {}", path)
		);
	}

	return content;
}

Tilemap loadTilemap(std::string_view path) {
	std::ifstream file(path.data(), std::ios::binary);
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
		config.seed = Seed::from_string(*options.seed);
	} else {
		config.seed = Seed::device_random();
	}

	return Tilemap::generate(config);
}

World createWorldFromOptions(const ProgramOptions &options) {
	Tilemap tilemap = options.map_file.empty()
		? generateTilemap(options)
		: loadTilemap(options.map_file);

	World world(std::move(tilemap));
	world.setPlayerProgram(
		1, loadBinary(options.p1_base), loadBinary(options.p1_unit)
	);
	world.setPlayerProgram(
		2, loadBinary(options.p2_base), loadBinary(options.p2_unit)
	);
	return world;
}

std::optional<std::ofstream> openReplayFile(const std::string &path) {
	if (path.empty()) {
		return std::nullopt;
	}

	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		throw std::runtime_error(
			std::format("Failed to open replay file: {}", path)
		);
	}
	return stream;
}

void writeChunk(std::ofstream &stream, std::span<const std::byte> chunk) {
	if (chunk.empty()) {
		return;
	}

	stream.write(
		reinterpret_cast<const char *>(chunk.data()),
		static_cast<std::streamsize>(chunk.size())
	);
	stream.flush();
	if (!stream.good()) {
		throw std::runtime_error("Failed to write replay file");
	}
}

} // namespace cr
