#include "corertt/cli_common.h"
#include "corertt/replay.h"
#include <atomic>
#include <csignal>
#include <iostream>
#include <limits>

namespace {

static std::atomic<bool> g_terminateRequested{false};

void signalHandler(int signum) noexcept {
	if (signum == SIGTERM || signum == SIGINT) {
		g_terminateRequested.store(true, std::memory_order_relaxed);
	}
}

int runHeadlessLiveMode(const cr::ProgramOptions &options) {
	std::signal(SIGTERM, signalHandler);
	std::signal(SIGINT, signalHandler);

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

	std::uint32_t tick_limit = options.max_ticks > 0
		? options.max_ticks
		: std::numeric_limits<std::uint32_t>::max();

	while (!world.gameOver() && world.currentTick() < tick_limit) {
		world.step();

		auto tick = cr::ReplayTickFrame::fromWorldState(world);
		auto tick_bytes = cr::ReplayTickFrame::encode(tick);
		cr::writeChunk(*replay_file_stream, tick_bytes);

		if (g_terminateRequested.load(std::memory_order_relaxed)) {
			std::println(std::cerr, "Termination signal received, quitting...");
			break;
		}
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
