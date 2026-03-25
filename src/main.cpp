#include "corertt/cli_common.h"
#include "corertt/plain_ui.h"
#include "corertt/replay.h"
#include "corertt/tui.h"
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>

namespace {

std::unique_ptr<cr::UIRunner> createUI(cr::UIMode mode) {
	switch (mode) {
	case cr::UIMode::Tui:
		return std::make_unique<cr::TuiRunner>();
	case cr::UIMode::Plain:
		return std::make_unique<cr::PlainUIRunner>(std::cout, std::cerr);
	}

	throw std::runtime_error("Unsupported UI mode");
}

int runPlaybackMode(const cr::ProgramOptions &options, cr::UIRunner &ui) {
	std::chrono::milliseconds step_interval(options.step_interval_ms);
	ui.start();

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
			ui.publishError(
				std::format(
					"Header decode error: {}",
					cr::DecodeErrorCode::MissingHeader
				)
			);
			return ui.wait();
		}

		auto header_result = decoder.readHeader();
		if (!header_result.has_value()) {
			ui.publishError(
				std::format("Header decode error: {}", header_result.error())
			);
			return ui.wait();
		}

		ui.publishHeader(decoder.header());

		while (!ui.shouldStop()) {
			auto next_tick_time = std::chrono::steady_clock::now()
				+ step_interval;

			if (decoder.ended()) {
				ui.publishEnd(decoder.endMarker());
				break;
			}

			if (decoder.canReadNextRecord()) {
				auto read_result = decoder.nextTick();
				if (read_result.status
				    == cr::ReplayStreamDecoder::ReadStatus::Error) {
					ui.publishError(
						std::format("Tick decode error: {}", *read_result.error)
					);
					break;
				}

				if (read_result.status
				    == cr::ReplayStreamDecoder::ReadStatus::Tick) {
					ui.publishTick(*read_result.tick);
				}

				if (read_result.status
				    == cr::ReplayStreamDecoder::ReadStatus::End) {
					ui.publishEnd(decoder.endMarker());
					break;
				}
			} else if (input.eof()) {
				ui.publishError(
					std::format(
						"Tick decode error: {}",
						cr::DecodeErrorCode::MissingEndMarker
					)
				);
				break;
			}

			std::this_thread::sleep_until(next_tick_time);
		}

		return ui.wait();
	} catch (...) {
		ui.requestStop();
		ui.wait();
		throw;
	}
}

int runLiveMode(const cr::ProgramOptions &options, cr::UIRunner &ui) {
	std::chrono::milliseconds step_interval{options.step_interval_ms};
	ui.start();

	try {
		cr::World world = cr::createWorldFromOptions(options);
		std::optional<std::ofstream> replay_file_stream = cr::openReplayFile(
			options.replay_file
		);

		auto header = cr::ReplayHeader::fromWorld(world);
		if (replay_file_stream.has_value()) {
			auto header_bytes = cr::ReplayHeader::encode(header);
			cr::writeChunk(*replay_file_stream, header_bytes);
		}

		ui.publishHeader(header);

		while (!ui.shouldStop()) {
			auto next_tick_time = std::chrono::steady_clock::now()
				+ step_interval;
			world.step();

			auto tick = cr::ReplayTickFrame::fromWorldState(world);
			if (replay_file_stream) {
				auto tick_bytes = cr::ReplayTickFrame::encode(tick);
				cr::writeChunk(*replay_file_stream, tick_bytes);
			}

			ui.publishTick(tick);

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

		ui.publishEnd(end_marker);
		return ui.wait();
	} catch (...) {
		ui.requestStop();
		ui.wait();
		throw;
	}
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

	if (options.step_interval_ms < 0) {
		std::cerr << "--step-interval-ms must be non-negative\n";
		return 1;
	}

	try {
		auto ui = createUI(options.ui_mode);
		if (!options.play_replay.empty()) {
			return runPlaybackMode(options, *ui);
		}
		return runLiveMode(options, *ui);
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
