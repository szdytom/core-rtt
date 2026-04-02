#include "corertt/build_config.h"
#include "corertt/cli.h"
#include "corertt/draw_judge.h"
#include "corertt/replay.h"
#include <atomic>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <limits>
#include <memory>
#include <print>
#include <zstd.h>

#if defined(__linux__) || defined(__unix__)                                  \
	|| (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__)     \
	|| defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) \
	|| defined(__CYGWIN__)
#include <unistd.h>
#else
#ifdef _WIN32
#include <fcntl.h>
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
	if (options.worker_mode) {
		std::println(
			std::cerr,
			"{{\"version\": \"{}\", \"commit\": \"{}\", \"zstd_version\": {}}}",
			cr::program_version, cr::git_commit_hash, ZSTD_versionNumber()
		);
	}

	std::signal(SIGTERM, signalHandler);
	std::signal(SIGINT, signalHandler);

	cr::World world = cr::createWorldFromOptions(options);

	auto replay_file_stream = cr::openReplayFile(options.replay_file);
	if (!replay_file_stream && isStdoutTTY()) {
		throw std::runtime_error(
			"Nowhere to write replay file (stdout is a TTY)"
		);
	}

#ifdef _WIN32
	if (!replay_file_stream) {
		_setmode(_fileno(stdout), _O_BINARY);
	}
#endif

	std::ostream &replay_stream = replay_file_stream
		? *replay_file_stream
		: std::cout;
	std::unique_ptr<cr::ReplayChunkWriter> replay_writer = options.output_zstd
		? cr::createZstdReplayChunkWriter(
			  replay_stream, options.output_zstd_level
		  )
		: cr::createRawReplayChunkWriter(replay_stream);
	std::unique_ptr<cr::RuleDrawJudge>
		rule_draw_judge = cr::createNoopRuleDrawJudge();
	if (options.dynamic_draw) {
		rule_draw_judge = cr::createDynamicTurnLimitRuleDrawJudge();
	} else if (options.max_ticks > 0) {
		rule_draw_judge = cr::createMaxTicksRuleDrawJudge(options.max_ticks);
	}

	auto header = cr::ReplayHeader::fromWorld(world);
	auto header_bytes = cr::ReplayHeader::encode(header);
	replay_writer->writeChunk(header_bytes);

	int exit_code = 0;
	while (!world.gameOver()) {
		world.step();

		auto tick = cr::ReplayTickFrame::fromWorldState(world);
		auto tick_bytes = cr::ReplayTickFrame::encode(tick);
		replay_writer->writeChunk(tick_bytes);

		if (world.gameOver()) {
			break;
		}

		if (g_terminateRequested.load(std::memory_order_relaxed)) {
			if (options.worker_mode) {
				std::println(
					std::cerr, "{{\"termination_signal_received\": true}}"
				);
			} else {
				std::println(
					std::cerr, "Termination signal received, quitting..."
				);
			}
			exit_code = 1;
			break;
		}

		if (rule_draw_judge->shouldDraw(world)) {
			world.markRuleDraw();
			break;
		}
	}

	auto end_marker = cr::ReplayEndMarker::fromWorld(world);
	auto end_bytes = cr::ReplayEndMarker::encode(end_marker);
	replay_writer->writeChunk(end_bytes);
	replay_writer->finish();

	if (options.worker_mode) {
		std::println(
			std::cerr,
			"{{\"termination\": \"{}\", \"winner_player_id\": {}, "
			"\"p1_base_crash\": {}, \"p1_unit_crash\": {}, "
			"\"p2_base_crash\": {}, \"p2_unit_crash\": {}}}",
			end_marker.termination == cr::ReplayTermination::Completed
				? "completed"
				: end_marker.termination == cr::ReplayTermination::RuleDraw
				? "rule-draw"
				: "aborted",
			end_marker.winner_player_id, world.player(1).base_elf_crash_flag,
			world.player(1).unit_elf_crash_flag,
			world.player(2).base_elf_crash_flag,
			world.player(2).unit_elf_crash_flag
		);
	}

	return exit_code;
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
