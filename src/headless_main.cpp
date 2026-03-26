#include "corertt/cli_common.h"
#include "corertt/replay.h"
#include <atomic>
#include <csignal>
#include <iostream>
#include <limits>

#if defined(__linux__) || defined(__unix__)                                  \
	|| (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__)     \
	|| defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) \
	|| defined(__CYGWIN__)
#include <unistd.h>
#else
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#endif
#endif

namespace {

static std::atomic<bool> g_terminateRequested{false};

void signalHandler(int signum) noexcept {
	if (signum == SIGTERM || signum == SIGINT) {
		g_terminateRequested.store(true, std::memory_order_relaxed);
	}
}

bool isStdoutTTY() noexcept {
	return isatty(fileno(stdout)) != 0;
}

int runHeadlessLiveMode(const cr::ProgramOptions &options) {
	std::signal(SIGTERM, signalHandler);
	std::signal(SIGINT, signalHandler);

	cr::World world = cr::createWorldFromOptions(options);

	auto replay_file_stream = cr::openReplayFile(options.replay_file);
	if (!replay_file_stream && isStdoutTTY()) {
		std::println(
			std::cerr, "Cannot write replay to stdout because it is a TTY."
		);
		throw std::runtime_error("Nowhere to write replay file");
	}

	std::ostream &replay_stream = replay_file_stream
		? *replay_file_stream
		: std::cout;

	auto header = cr::ReplayHeader::fromWorld(world);
	auto header_bytes = cr::ReplayHeader::encode(header);
	cr::writeChunk(replay_stream, header_bytes);

	std::uint32_t tick_limit = options.max_ticks > 0
		? options.max_ticks
		: std::numeric_limits<std::uint32_t>::max();

	while (!world.gameOver() && world.currentTick() < tick_limit) {
		world.step();

		auto tick = cr::ReplayTickFrame::fromWorldState(world);
		auto tick_bytes = cr::ReplayTickFrame::encode(tick);
		cr::writeChunk(replay_stream, tick_bytes);

		if (g_terminateRequested.load(std::memory_order_relaxed)) {
			std::println(std::cerr, "Termination signal received, quitting...");
			break;
		}
	}

	auto end_marker = cr::ReplayEndMarker::fromWorld(world);
	auto end_bytes = cr::ReplayEndMarker::encode(end_marker);
	cr::writeChunk(replay_stream, end_bytes);

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
