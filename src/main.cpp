#include "corertt/build_config.h"
#include "corertt/replay.h"
#include "corertt/tilemap.h"
#include "corertt/tui.h"
#include "corertt/world.h"
#include <argparse/argparse.hpp>
#include <array>
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct ProgramOptions {
	int width = 64;
	int height = 64;
	int base_size = 5;
	int resources = 5;
	int step_interval_ms = 200;
	std::string p1_base = "player1_base.elf";
	std::string p1_unit = "player1_unit.elf";
	std::string p2_base = "player2_base.elf";
	std::string p2_unit = "player2_unit.elf";
	std::optional<std::string> seed;
	std::string replay_file;
	std::string play_replay;
};

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

void writeChunk(std::ofstream *stream, std::span<const std::byte> chunk) {
	if (stream == nullptr || chunk.empty()) {
		return;
	}

	stream->write(
		reinterpret_cast<const char *>(chunk.data()),
		static_cast<std::streamsize>(chunk.size())
	);
	stream->flush();
	if (!stream->good()) {
		throw std::runtime_error("Failed to write replay file");
	}
}

ProgramOptions parseOptions(int argc, char *argv[]) {
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
	program.add_argument("-s", "--seed")
		.help("seed for map generation (omitted means device-random)");
	program.add_argument("--replay-file")
		.default_value(std::string(""))
		.help("optional path to write binary replay in live mode");
	program.add_argument("--play-replay")
		.default_value(std::string(""))
		.help("optional replay file path for playback mode");

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
	if (program.is_used("--seed")) {
		options.seed = program.get<std::string>("--seed");
	}
	options.replay_file = program.get<std::string>("--replay-file");
	options.play_replay = program.get<std::string>("--play-replay");
	return options;
}

int runPlaybackMode(const ProgramOptions &options) {
	std::chrono::milliseconds step_interval(options.step_interval_ms);
	cr::SynchronizedReplayProgress replay_sync;
	cr::ReplayStreamDecoder decoder;
	cr::TuiRunner tui_runner(replay_sync);
	std::jthread producer_thread([&](std::stop_token stop_token) {
		try {
			std::ifstream input(options.play_replay, std::ios::binary);
			if (!input.is_open()) {
				throw std::runtime_error(
					std::format(
						"Failed to open replay file for playback: {}",
						options.play_replay
					)
				);
			}

			std::array<char, 4096> raw_chunk{};
			while (!stop_token.stop_requested()) {
				auto next_tick_time = std::chrono::steady_clock::now()
					+ step_interval;

				while (!decoder.ended() && !decoder.canReadNextRecord()
				       && input.good() && !stop_token.stop_requested()) {
					// Read up to 4096 bytes at a time and feed to the decoder
					input.read(raw_chunk.data(), raw_chunk.size());
					std::size_t bytes_read = input.gcount();

					decoder.pushBytes({raw_chunk.data(), bytes_read});

					if (!decoder.hasHeader() && decoder.canReadHeader()) {
						std::lock_guard lock(replay_sync.mutex);
						auto res = decoder.readHeader();
						if (!res) {
							replay_sync.last_error = std::format(
								"Header decode error: {}", res.error()
							);
							goto end_production;
						}

						replay_sync.progress.phase = cr::ReplayParsePhase::Tick;
						replay_sync.progress.header = decoder.header();
					}
				}

				if (stop_token.stop_requested()) {
					break;
				}

				if (decoder.ended()) {
					std::lock_guard lock(replay_sync.mutex);
					replay_sync.progress.phase = cr::ReplayParsePhase::End;
					replay_sync.progress.end_marker = decoder.endMarker();
					break;
				}

				if (decoder.canReadNextRecord()) {
					auto read_result = decoder.nextTick();
					if (read_result.status
					    == cr::ReplayStreamDecoder::ReadStatus::Error) {
						std::lock_guard lock(replay_sync.mutex);
						replay_sync.last_error = std::format(
							"Tick decode error: {}", *read_result.error
						);
						goto end_production;
					}

					if (read_result.status
					    == cr::ReplayStreamDecoder::ReadStatus::Tick) {
						std::lock_guard lock(replay_sync.mutex);
						replay_sync.progress.current_tick = *read_result.tick;
					}

					if (read_result.status
					    == cr::ReplayStreamDecoder::ReadStatus::End) {
						std::lock_guard lock(replay_sync.mutex);
						replay_sync.progress.phase = cr::ReplayParsePhase::End;
						replay_sync.progress.end_marker = decoder.endMarker();
						break;
					}
				}

				tui_runner.notifyUpdate();
				std::this_thread::sleep_until(next_tick_time);
			}
		} catch (const std::exception &e) {
			std::cerr << e.what() << '\n';
		}

	end_production:
		tui_runner.notifyUpdate();
	});

	const int tui_exit_code = tui_runner.run();
	producer_thread.request_stop();
	return tui_exit_code;
}

int runLiveMode(const ProgramOptions &options) {
	std::chrono::milliseconds step_interval{options.step_interval_ms};
	cr::TilemapGenerationConfig config;
	config.width = options.width;
	config.height = options.height;
	config.base_size = options.base_size;
	config.num_resources = options.resources;
	if (options.seed.has_value()) {
		config.seed = cr::Seed::from_string(*options.seed);
	} else {
		config.seed = cr::Seed::device_random();
	}

	auto world = std::make_shared<cr::World>(cr::Tilemap::generate(config));
	world->setPlayerProgram(
		1, loadBinary(options.p1_base), loadBinary(options.p1_unit)
	);
	world->setPlayerProgram(
		2, loadBinary(options.p2_base), loadBinary(options.p2_unit)
	);

	cr::SynchronizedReplayProgress replay_sync;
	cr::TuiRunner tui_runner(replay_sync);

	std::shared_ptr<std::ofstream> replay_file_stream;
	if (!options.replay_file.empty()) {
		replay_file_stream = std::make_shared<std::ofstream>(
			options.replay_file, std::ios::binary | std::ios::trunc
		);
		if (!replay_file_stream->is_open()) {
			throw std::runtime_error(
				std::format(
					"Failed to open replay file: {}", options.replay_file
				)
			);
		}
	}

	auto header = cr::ReplayHeader::fromWorld(*world);
	auto header_bytes = cr::ReplayHeader::encode(header);

	{
		std::lock_guard lock(replay_sync.mutex);
		replay_sync.progress.phase = cr::ReplayParsePhase::Tick;
		replay_sync.progress.header = std::move(header);
	}
	tui_runner.notifyUpdate();
	writeChunk(replay_file_stream.get(), header_bytes);

	std::jthread producer_thread([&](std::stop_token stop_token) {
		try {
			while (!stop_token.stop_requested()) {
				auto next_tick_time = std::chrono::steady_clock::now()
					+ step_interval;
				world->step();
				auto tick = cr::ReplayTickFrame::fromWorldState(*world);
				auto tick_bytes = cr::ReplayTickFrame::encode(tick);

				{
					std::lock_guard lock(replay_sync.mutex);
					replay_sync.progress.current_tick = tick;
				}
				tui_runner.notifyUpdate();
				writeChunk(replay_file_stream.get(), tick_bytes);

				next_tick_time += step_interval;
				if (world->gameOver()) {
					break;
				}
				std::this_thread::sleep_until(next_tick_time);
			}

			auto end_marker = cr::ReplayEndMarker::fromWorld(*world);
			auto end_bytes = cr::ReplayEndMarker::encode(end_marker);

			{
				std::lock_guard lock(replay_sync.mutex);
				replay_sync.progress.phase = cr::ReplayParsePhase::End;
				replay_sync.progress.end_marker = end_marker;
			}
			tui_runner.notifyUpdate();
			writeChunk(replay_file_stream.get(), end_bytes);
		} catch (const std::exception &e) {
			std::cerr << e.what() << '\n';
		}

		tui_runner.notifyUpdate();
	});

	const int tui_exit_code = tui_runner.run();
	producer_thread.request_stop();
	return tui_exit_code;
}

} // namespace

int main(int argc, char *argv[]) {
	ProgramOptions options;
	try {
		options = parseOptions(argc, argv);
	} catch (...) {
		return 1;
	}

	if (options.step_interval_ms <= 0) {
		std::cerr << "--step-interval-ms must be positive\n";
		return 1;
	}

	try {
		if (!options.play_replay.empty()) {
			return runPlaybackMode(options);
		}
		return runLiveMode(options);
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
