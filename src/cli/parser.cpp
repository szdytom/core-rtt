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

	if (mode == CliMode::Headless) {
		program.add_argument("--max-ticks")
			.default_value(0u)
			.scan<'u', unsigned int>()
			.help(
				"maximum number of ticks to simulate before auto draw "
				"(0=unlimited)"
			);
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

	if (mode == CliMode::Headless) {
		options.max_ticks = program.get<unsigned int>("--max-ticks");
		options.worker_mode = program.get<bool>("--worker-mode");
	}

	if (mode == CliMode::Interactive) {
		options.step_interval_ms = program.get<int>("--step-interval-ms");
		options.play_replay = program.get<std::string>("--play-replay");
		options.ui_mode = parseUIMode(program.get<std::string>("--ui-mode"));
	}

	options.output_zstd = program.get<bool>("--output-zstd");
	options.output_zstd_level = FIXED_ZSTD_COMPRESSION_LEVEL;

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

	return options;
}

} // namespace cr
