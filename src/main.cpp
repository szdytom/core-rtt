#include "corertt/log.h"
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

namespace {

class RuntimeLogFileMirror {
public:
	explicit RuntimeLogFileMirror(const std::string &path): _path(path) {
		_stream.open(path, std::ios::out | std::ios::trunc);
		if (!_stream.is_open()) {
			throw std::runtime_error(
				std::format("Failed to open log file: {}", path)
			);
		}
	}

	void append(const cr::LogEntry &entry) noexcept {
		if (_write_failed) {
			return;
		}

		try {
			const auto lines = cr::formatLogEntryLines(entry);
			for (const auto &line : lines) {
				_stream << line << '\n';
			}
			_stream.flush();
			if (!_stream.good()) {
				throw std::runtime_error(
					std::format("Failed to write runtime logs to {}", _path)
				);
			}
		} catch (const std::exception &e) {
			std::cerr << e.what() << '\n';
			_write_failed = true;
		} catch (...) {
			std::cerr << "Unknown error while writing runtime logs\n";
			_write_failed = true;
		}
	}

private:
	std::ofstream _stream;
	std::string _path;
	bool _write_failed = false;
};

} // namespace

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
	argparse::ArgumentParser program("corertt");

	program.add_argument("--width")
		.default_value(64)
		.scan<'i', int>()
		.help("tilemap width");
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
		.default_value(std::string("test"))
		.help("seed for map generation");
	program.add_argument("--log-file")
		.default_value(std::string(""))
		.help("optional path to mirror runtime logs to file");

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
	config.seed = cr::Seed::from_string(program.get<std::string>("--seed"));
	const int step_interval_ms = program.get<int>("--step-interval-ms");
	if (step_interval_ms <= 0) {
		std::cerr << "--step-interval-ms must be positive\n";
		return 1;
	}

	try {
		auto world = std::make_unique<cr::World>(cr::Tilemap::generate(config));

		std::shared_ptr<RuntimeLogFileMirror> log_mirror;
		const auto log_file_path = program.get<std::string>("--log-file");
		if (!log_file_path.empty()) {
			log_mirror = std::make_shared<RuntimeLogFileMirror>(log_file_path);
			world->setRuntimeLogSink(
				[log_mirror](const cr::LogEntry &entry) {
					log_mirror->append(entry);
				}
			);
		}

		world->setPlayerProgram(
			1, loadBinary(program.get<std::string>("--p1-base")),
			loadBinary(program.get<std::string>("--p1-unit"))
		);
		world->setPlayerProgram(
			2, loadBinary(program.get<std::string>("--p2-base")),
			loadBinary(program.get<std::string>("--p2-unit"))
		);

		return cr::runTui(*world, std::chrono::milliseconds(step_interval_ms));
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
