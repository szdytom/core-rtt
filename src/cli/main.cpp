#include "corertt/cli.h"
#include "corertt/draw_judge.h"
#include "corertt/plain_ui.h"
#include "corertt/replay.h"
#include "corertt/tui.h"
#include <chrono>
#include <filesystem>
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

std::string computeTilemapSourceLabel(const cr::ProgramOptions &options) {
	if (!options.map_file.empty()) {
		return std::format(
			"map file: {}",
			std::filesystem::path(options.map_file).filename().string()
		);
	}

	if (options.seed.has_value()) {
		return std::format("seed: {}", options.seed->toString());
	}

	return "seed: (unknown)";
}

std::string computePlaybackTilemapSourceLabel(
	const cr::ProgramOptions &options
) {
	return std::format(
		"replay: {}",
		std::filesystem::path(options.play_replay).filename().string()
	);
}

int runPlaybackMode(
	const cr::ProgramOptions &options, cr::UIRunner &ui,
	cr::TuiRunner *tui_runner
) {
	std::chrono::milliseconds step_interval(options.step_interval_ms);
	ui.start();
	if (tui_runner != nullptr) {
		tui_runner->publishTilemapSource(
			computePlaybackTilemapSourceLabel(options)
		);
	}

	int exit_code = 0;
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
			ui.wait();
			return 1;
		}

		auto header_result = decoder.readHeader();
		if (!header_result.has_value()) {
			ui.publishError(
				std::format("Header decode error: {}", header_result.error())
			);
			ui.wait();
			return 1;
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
					exit_code = 1;
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
				exit_code = 1;
				break;
			} else if (!input.good()) {
				ui.publishError(
					std::format(
						"I/O error while reading replay file: {}",
						options.play_replay
					)
				);
				exit_code = 1;
				break;
			}

			std::this_thread::sleep_until(next_tick_time);
		}

		ui.wait();
		return exit_code;
	} catch (...) {
		ui.requestStop();
		ui.wait();
		throw;
	}
}

int runLiveMode(
	const cr::ProgramOptions &options, cr::UIRunner &ui,
	cr::TuiRunner *tui_runner
) {
	std::chrono::milliseconds step_interval{options.step_interval_ms};
	ui.start();
	if (tui_runner != nullptr) {
		tui_runner->publishTilemapSource(computeTilemapSourceLabel(options));
	}

	try {
		auto world = cr::createWorldFromOptions(options);
		auto rule_draw_judge = cr::createRuleDrawJudgeFromOptions(options);
		auto replay_file_stream = cr::openReplayFile(options.replay_file);
		std::unique_ptr<cr::ReplayChunkWriter> replay_writer;
		if (replay_file_stream.has_value()) {
			replay_writer = options.output_zstd
				? cr::createZstdReplayChunkWriter(
					  *replay_file_stream, options.output_zstd_level
				  )
				: cr::createRawReplayChunkWriter(*replay_file_stream);
		}

		auto header = cr::ReplayHeader::fromWorld(world);
		if (replay_writer != nullptr) {
			auto header_bytes = cr::ReplayHeader::encode(header);
			replay_writer->writeChunk(header_bytes);
		}

		ui.publishHeader(header);

		while (!ui.shouldStop()) {
			auto next_tick_time = std::chrono::steady_clock::now()
				+ step_interval;
			world.step();

			auto tick = cr::ReplayTickFrame::fromWorldState(world);
			if (replay_writer != nullptr) {
				auto tick_bytes = cr::ReplayTickFrame::encode(tick);
				replay_writer->writeChunk(tick_bytes);
			}

			ui.publishTick(tick);

			if (world.gameOver()) {
				break;
			}

			if (rule_draw_judge->shouldDraw(world)) {
				world.markRuleDraw();
				break;
			}
			std::this_thread::sleep_until(next_tick_time);
		}

		auto end_marker = cr::ReplayEndMarker::fromWorld(world);
		if (replay_writer != nullptr) {
			auto end_bytes = cr::ReplayEndMarker::encode(end_marker);
			replay_writer->writeChunk(end_bytes);
			replay_writer->finish();
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
		auto *tui_runner = dynamic_cast<cr::TuiRunner *>(ui.get());
		if (!options.play_replay.empty()) {
			return runPlaybackMode(options, *ui, tui_runner);
		}
		return runLiveMode(options, *ui, tui_runner);
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
