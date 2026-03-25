#include "corertt/cli_common.h"
#include "corertt/replay.h"
#include "corertt/tui.h"
#include <chrono>
#include <format>
#include <iostream>
#include <optional>
#include <thread>

namespace {

int runPlaybackMode(const cr::ProgramOptions &options) {
	std::chrono::milliseconds step_interval(options.step_interval_ms);
	cr::SynchronizedReplayProgress replay_sync;
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

			cr::ReplayStreamDecoder decoder(input);
			if (!decoder.canReadHeader()) {
				std::lock_guard lock(replay_sync.mutex);
				replay_sync.last_error = std::format(
					"Header decode error: {}",
					cr::DecodeErrorCode::MissingHeader
				);
				goto end_production;
			}

			auto header_result = decoder.readHeader();
			if (!header_result.has_value()) {
				std::lock_guard lock(replay_sync.mutex);
				replay_sync.last_error = std::format(
					"Header decode error: {}", header_result.error()
				);
				goto end_production;
			}

			{
				std::lock_guard lock(replay_sync.mutex);
				replay_sync.progress.phase = cr::ReplayParsePhase::Tick;
				replay_sync.progress.header = decoder.header();
			}
			tui_runner.notifyUpdate();

			while (!stop_token.stop_requested()) {
				auto next_tick_time = std::chrono::steady_clock::now()
					+ step_interval;

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
					} else if (
						read_result.status
						== cr::ReplayStreamDecoder::ReadStatus::Tick
					) {
						std::lock_guard lock(replay_sync.mutex);
						replay_sync.progress.current_tick = *read_result.tick;
					} else if (
						read_result.status
						== cr::ReplayStreamDecoder::ReadStatus::End
					) {
						std::lock_guard lock(replay_sync.mutex);
						replay_sync.progress.phase = cr::ReplayParsePhase::End;
						replay_sync.progress.end_marker = decoder.endMarker();
						break;
					}
				} else if (input.eof()) {
					std::lock_guard lock(replay_sync.mutex);
					replay_sync.last_error = std::format(
						"Tick decode error: {}",
						cr::DecodeErrorCode::MissingEndMarker
					);
					goto end_production;
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

int runLiveMode(const cr::ProgramOptions &options) {
	std::chrono::milliseconds step_interval{options.step_interval_ms};
	cr::World world = cr::createWorldFromOptions(options);

	cr::SynchronizedReplayProgress replay_sync;
	cr::TuiRunner tui_runner(replay_sync);

	std::optional<std::ofstream> replay_file_stream = cr::openReplayFile(
		options.replay_file
	);

	auto header = cr::ReplayHeader::fromWorld(world);
	if (replay_file_stream.has_value()) {
		auto header_bytes = cr::ReplayHeader::encode(header);
		cr::writeChunk(*replay_file_stream, header_bytes);
	}

	{
		std::lock_guard lock(replay_sync.mutex);
		replay_sync.progress.phase = cr::ReplayParsePhase::Tick;
		replay_sync.progress.header = std::move(header);
	}
	tui_runner.notifyUpdate();

	std::jthread producer_thread([&](std::stop_token stop_token) {
		try {
			while (!stop_token.stop_requested()) {
				auto next_tick_time = std::chrono::steady_clock::now()
					+ step_interval;
				world.step();

				auto tick = cr::ReplayTickFrame::fromWorldState(world);
				if (replay_file_stream) {
					auto tick_bytes = cr::ReplayTickFrame::encode(tick);
					cr::writeChunk(*replay_file_stream, tick_bytes);
				}

				{
					std::lock_guard lock(replay_sync.mutex);
					replay_sync.progress.current_tick = std::move(tick);
				}
				tui_runner.notifyUpdate();

				if (world.gameOver()) {
					break;
				}
				std::this_thread::sleep_until(next_tick_time);
			}

			auto end_marker = cr::ReplayEndMarker::fromWorld(world);
			if (replay_file_stream) {
				auto end_bytes = cr::ReplayEndMarker::encode(end_marker);
				cr::writeChunk(*replay_file_stream, end_bytes);
			}

			{
				std::lock_guard lock(replay_sync.mutex);
				replay_sync.progress.phase = cr::ReplayParsePhase::End;
				replay_sync.progress.end_marker = end_marker;
			}
			tui_runner.notifyUpdate();
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
	cr::ProgramOptions options;
	try {
		options = cr::parseOptions(
			argc, argv, "corertt", cr::CliMode::Interactive
		);
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
