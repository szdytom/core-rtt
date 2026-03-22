#include "corertt/build_config.h"
#include "corertt/replay.h"
#include "corertt/tilemap.h"
#include "corertt/tui.h"
#include "corertt/world.h"
#include <argparse/argparse.hpp>
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

std::vector<std::uint8_t> loadBinary(std::string_view path) {
	std::ifstream file(path.data(), std::ios::binary);
	if (!file) {
		throw std::runtime_error(
			std::format("Failed to open ELF file: {}", path)
		);
	}

	return std::vector<std::uint8_t>(
		(std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()
	);
}

int main(int argc, char *argv[]) {
	argparse::ArgumentParser program(
		"corertt", std::string(cr::program_version_with_commit)
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
		.help("world step interval in milliseconds");
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
	program.add_argument("-s", "--seed")
		.help("seed for map generation (omitted means device-random)");
	program.add_argument("--replay-file")
		.default_value(std::string(""))
		.help("optional path to write binary replay");

	try {
		program.parse_args(argc, argv);
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		std::cerr << program;
		return 1;
	}

	cr::TilemapGenerationConfig config;
	config.width = program.get<int>("--width");
	config.height = program.get<int>("--height");
	config.base_size = program.get<int>("--base-size");
	config.num_resources = program.get<int>("--resources");
	if (program.is_used("--seed")) {
		config.seed = cr::Seed::from_string(program.get<std::string>("--seed"));
	} else {
		config.seed = cr::Seed::device_random();
	}
	const int step_interval_ms = program.get<int>("--step-interval-ms");
	if (step_interval_ms <= 0) {
		std::cerr << "--step-interval-ms must be positive\n";
		return 1;
	}

	try {
		auto world = std::make_unique<cr::World>(cr::Tilemap::generate(config));

		world->setPlayerProgram(
			1, loadBinary(program.get<std::string>("--p1-base")),
			loadBinary(program.get<std::string>("--p1-unit"))
		);
		world->setPlayerProgram(
			2, loadBinary(program.get<std::string>("--p2-base")),
			loadBinary(program.get<std::string>("--p2-unit"))
		);

		std::unique_ptr<cr::ReplayRecorder> replay_recorder;
		std::unique_ptr<cr::ReplayStreamEncoder> replay_encoder;
		std::unique_ptr<std::ofstream> replay_stream;

		const auto replay_file_path = program.get<std::string>("--replay-file");
		if (!replay_file_path.empty()) {
			replay_recorder = std::make_unique<cr::ReplayRecorder>(*world);
			replay_encoder = std::make_unique<cr::ReplayStreamEncoder>();
			replay_stream = std::make_unique<std::ofstream>(
				replay_file_path, std::ios::binary | std::ios::trunc
			);
			if (!replay_stream->is_open()) {
				throw std::runtime_error(
					std::format(
						"Failed to open replay file: {}", replay_file_path
					)
				);
			}

			const auto header_bytes = replay_encoder->encodeHeader(
				replay_recorder->header()
			);
			replay_stream->write(
				reinterpret_cast<const char *>(header_bytes.data()),
				static_cast<std::streamsize>(header_bytes.size())
			);
			replay_stream->flush();
		}

		return cr::runTui(
			*world, std::chrono::milliseconds(step_interval_ms),
			[&](cr::World &tick_world) {
			if (!replay_recorder || !replay_encoder || !replay_stream) {
				return;
			}

			replay_recorder->addTick(tick_world);
			const auto &tick = replay_recorder->ticks().back();
			const auto tick_bytes = replay_encoder->encodeTick(tick);
			replay_stream->write(
				reinterpret_cast<const char *>(tick_bytes.data()),
				static_cast<std::streamsize>(tick_bytes.size())
			);
			replay_stream->flush();

			if (tick_world.gameOver()) {
				const auto end_bytes = replay_encoder->encodeEnd();
				replay_stream->write(
					reinterpret_cast<const char *>(end_bytes.data()),
					static_cast<std::streamsize>(end_bytes.size())
				);
				replay_stream->flush();
			}
		}
		);
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
