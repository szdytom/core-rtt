#include "corertt/cli_common.h"
#include "corertt/build_config.h"
#include <argparse/argparse.hpp>
#include <array>
#include <charconv>
#include <format>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include <zstd.h>

namespace cr {

namespace {

constexpr int DEFAULT_ZSTD_COMPRESSION_LEVEL = 3;

struct OutputZstdCliState {
	std::vector<std::string> filtered_args;
	bool enabled = false;
	std::optional<int> level;
};

bool tryParseInt(std::string_view input, int &value) {
	if (input.empty()) {
		return false;
	}

	int parsed_value = 0;
	const auto *begin = input.data();
	const auto *end = input.data() + input.size();
	const auto parse_result = std::from_chars(begin, end, parsed_value);
	if (parse_result.ec != std::errc() || parse_result.ptr != end) {
		return false;
	}

	value = parsed_value;
	return true;
}

OutputZstdCliState preprocessOutputZstdArgs(int argc, char *argv[]) {
	OutputZstdCliState result;
	result.filtered_args.reserve(static_cast<std::size_t>(argc));
	if (argc > 0) {
		result.filtered_args.emplace_back(argv[0]);
	}

	for (int i = 1; i < argc; ++i) {
		const std::string_view arg(argv[i]);
		if (arg == "-z" || arg == "--output-zstd") {
			result.enabled = true;
			if (i + 1 < argc) {
				int level = 0;
				if (tryParseInt(argv[i + 1], level)) {
					result.level = level;
					++i;
				}
			}
			continue;
		}

		if (arg.starts_with("-z=")) {
			result.enabled = true;
			int level = 0;
			if (!tryParseInt(arg.substr(3), level)) {
				throw std::runtime_error(
					std::format("Invalid zstd compression level: '{}'", arg)
				);
			}
			result.level = level;
			continue;
		}

		if (arg.starts_with("--output-zstd=")) {
			result.enabled = true;
			int level = 0;
			if (!tryParseInt(arg.substr(14), level)) {
				throw std::runtime_error(
					std::format("Invalid zstd compression level: '{}'", arg)
				);
			}
			result.level = level;
			continue;
		}

		result.filtered_args.emplace_back(arg);
	}

	return result;
}

void validateZstdCompressionLevel(int compression_level) {
	const int min_level = ZSTD_minCLevel();
	const int max_level = ZSTD_maxCLevel();
	if (compression_level < min_level || compression_level > max_level) {
		throw std::runtime_error(
			std::format(
				"Invalid zstd compression level {} (supported range: {}..{})",
				compression_level, min_level, max_level
			)
		);
	}
}

void writeBytesToStream(
	std::ostream &stream, const char *data, std::size_t size
) {
	if (size == 0) {
		return;
	}

	stream.write(data, static_cast<std::streamsize>(size));
	if (!stream.good()) {
		throw std::runtime_error("Failed to write replay file");
	}
}

class RawReplayChunkWriter final : public ReplayChunkWriter {
public:
	explicit RawReplayChunkWriter(std::ostream &stream): _stream(stream) {}

	void writeChunk(std::span<const std::byte> chunk) override {
		if (chunk.empty()) {
			return;
		}

		writeBytesToStream(
			_stream, reinterpret_cast<const char *>(chunk.data()), chunk.size()
		);
		_stream.flush();
		if (!_stream.good()) {
			throw std::runtime_error("Failed to flush replay file");
		}
	}

	void finish() override {
		_stream.flush();
		if (!_stream.good()) {
			throw std::runtime_error("Failed to flush replay file");
		}
	}

private:
	std::ostream &_stream;
};

class ZstdReplayChunkWriter final : public ReplayChunkWriter {
public:
	ZstdReplayChunkWriter(std::ostream &stream, int compression_level)
		: _stream(stream), _out_buffer(ZSTD_CStreamOutSize()) {
		validateZstdCompressionLevel(compression_level);

		_context = ZSTD_createCStream();
		if (_context == nullptr) {
			throw std::runtime_error(
				"Failed to create zstd compression stream"
			);
		}

		const std::size_t init_result = ZSTD_initCStream(
			_context, compression_level
		);
		if (ZSTD_isError(init_result) != 0U) {
			throw std::runtime_error(
				std::format(
					"Failed to initialize zstd compression stream: {}",
					ZSTD_getErrorName(init_result)
				)
			);
		}
	}

	~ZstdReplayChunkWriter() override {
		if (_context != nullptr) {
			if (!_finished) {
				try {
					finish();
				} catch (...) {}
			}
			ZSTD_freeCStream(_context);
		}
	}

	void writeChunk(std::span<const std::byte> chunk) override {
		if (chunk.empty()) {
			return;
		}

		ZSTD_inBuffer input_buffer = {
			.src = chunk.data(),
			.size = chunk.size(),
			.pos = 0,
		};

		while (input_buffer.pos < input_buffer.size) {
			ZSTD_outBuffer output_buffer = {
				.dst = _out_buffer.data(),
				.size = _out_buffer.size(),
				.pos = 0,
			};

			const std::size_t compress_result = ZSTD_compressStream2(
				_context, &output_buffer, &input_buffer, ZSTD_e_continue
			);
			if (ZSTD_isError(compress_result) != 0U) {
				throw std::runtime_error(
					std::format(
						"zstd replay output failed: {}",
						ZSTD_getErrorName(compress_result)
					)
				);
			}

			writeBytesToStream(
				_stream, reinterpret_cast<const char *>(_out_buffer.data()),
				output_buffer.pos
			);
		}
	}

	void finish() override {
		if (_finished) {
			return;
		}

		ZSTD_inBuffer input_buffer = {
			.src = nullptr,
			.size = 0,
			.pos = 0,
		};

		std::size_t remaining = 1;
		while (remaining != 0) {
			ZSTD_outBuffer output_buffer = {
				.dst = _out_buffer.data(),
				.size = _out_buffer.size(),
				.pos = 0,
			};

			remaining = ZSTD_compressStream2(
				_context, &output_buffer, &input_buffer, ZSTD_e_end
			);
			if (ZSTD_isError(remaining) != 0U) {
				throw std::runtime_error(
					std::format(
						"zstd replay output failed: {}",
						ZSTD_getErrorName(remaining)
					)
				);
			}

			writeBytesToStream(
				_stream, reinterpret_cast<const char *>(_out_buffer.data()),
				output_buffer.pos
			);
		}

		_stream.flush();
		if (!_stream.good()) {
			throw std::runtime_error("Failed to flush replay file");
		}
		_finished = true;
	}

private:
	std::ostream &_stream;
	ZSTD_CStream *_context = nullptr;
	std::vector<std::byte> _out_buffer;
	bool _finished = false;
};

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
	const auto output_zstd_state = preprocessOutputZstdArgs(argc, argv);

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
		.default_value(false)
		.implicit_value(true)
		.help(
			"compress replay output with zstd; LEVEL is optional via "
			"-z=LEVEL or --output-zstd=LEVEL"
		);

	if (mode == CliMode::Headless) {
		program.add_argument("--max-ticks")
			.default_value(0u)
			.scan<'u', unsigned int>()
			.help(
				"maximum number of ticks to simulate before auto draw "
				"(0=unlimited)"
			);
		program.add_argument("--worker-mode")
			.default_value(false)
			.implicit_value(true)
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
		program.parse_args(output_zstd_state.filtered_args);
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

	options.output_zstd = output_zstd_state.enabled;
	if (output_zstd_state.level.has_value()) {
		options.output_zstd_level = *output_zstd_state.level;
	} else {
		options.output_zstd_level = DEFAULT_ZSTD_COMPRESSION_LEVEL;
	}
	if (options.output_zstd) {
		validateZstdCompressionLevel(options.output_zstd_level);
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
std::unique_ptr<ReplayChunkWriter> createRawReplayChunkWriter(
	std::ostream &stream
) {
	return std::make_unique<RawReplayChunkWriter>(stream);
}

std::unique_ptr<ReplayChunkWriter> createZstdReplayChunkWriter(
	std::ostream &stream, int compression_level
) {
	return std::make_unique<ZstdReplayChunkWriter>(stream, compression_level);
}

} // namespace cr
