#ifndef CORERTT_CLI_COMMON_H
#define CORERTT_CLI_COMMON_H

#include "corertt/tilemap.h"
#include "corertt/ui.h"
#include "corertt/world.h"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iosfwd>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cr {

enum class CliMode {
	Interactive,
	Headless,
};

struct ProgramOptions {
	int width = 64;
	int height = 64;
	int base_size = 5;
	int resources = 5;
	int step_interval_ms = 200;
	std::uint32_t max_ticks = 0; // 0 means no max tick limit
	std::string p1_base = "player1_base.elf";
	std::string p1_unit = "player1_unit.elf";
	std::string p2_base = "player2_base.elf";
	std::string p2_unit = "player2_unit.elf";
	std::string map_file;
	std::optional<Seed> seed;
	std::string replay_file;
	std::string play_replay;
	bool output_zstd = false;
	int output_zstd_level = 3;
	bool worker_mode = false;
	UIMode ui_mode = UIMode::Tui;
};

class ReplayChunkWriter {
public:
	virtual ~ReplayChunkWriter() = default;
	virtual void writeChunk(std::span<const std::byte> chunk) = 0;
	virtual void finish() = 0;
};

ProgramOptions parseOptions(
	int argc, char *argv[], const std::string &program_name, CliMode mode
);

std::vector<std::uint8_t> loadBinary(std::string_view path);
Tilemap loadTilemap(std::string_view path);
Tilemap generateTilemap(const ProgramOptions &options);
World createWorldFromOptions(const ProgramOptions &options);

std::optional<std::ofstream> openReplayFile(const std::string &path);
std::unique_ptr<ReplayChunkWriter> createRawReplayChunkWriter(
	std::ostream &stream
);
std::unique_ptr<ReplayChunkWriter> createZstdReplayChunkWriter(
	std::ostream &stream, int compression_level
);

} // namespace cr

#endif // CORERTT_CLI_COMMON_H
