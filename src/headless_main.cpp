#include "corertt/cli_common.h"
#include "corertt/replay.h"
#include <iostream>

namespace {

int runHeadlessLiveMode(const cr::ProgramOptions &options) {
	cr::World world = cr::createWorldFromOptions(options);
	auto replay_file_stream = cr::openReplayFile(options.replay_file);
	if (!replay_file_stream) {
		throw std::runtime_error(
			"Replay file path is required in headless mode"
		);
	}

	auto header = cr::ReplayHeader::fromWorld(world);
	auto header_bytes = cr::ReplayHeader::encode(header);
	cr::writeChunk(*replay_file_stream, header_bytes);

	while (!world.gameOver()) {
		world.step();
		auto tick = cr::ReplayTickFrame::fromWorldState(world);
		auto tick_bytes = cr::ReplayTickFrame::encode(tick);
		cr::writeChunk(*replay_file_stream, tick_bytes);
	}

	auto end_marker = cr::ReplayEndMarker::fromWorld(world);
	auto end_bytes = cr::ReplayEndMarker::encode(end_marker);
	cr::writeChunk(*replay_file_stream, end_bytes);

	return 0;
}

} // namespace

int main(int argc, char *argv[]) {
	cr::ProgramOptions options;
	try {
		options = cr::parseOptions(
			argc, argv, "corertt_headless", cr::CliMode::Headless
		);
	} catch (...) {
		return 1;
	}

	try {
		return runHeadlessLiveMode(options);
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
