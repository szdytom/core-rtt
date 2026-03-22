#ifndef CORERTT_TUI_H
#define CORERTT_TUI_H

#include "corertt/replay.h"
#include <chrono>
#include <ftxui/component/screen_interactive.hpp>

namespace cr {

class TuiRunner {
public:
	TuiRunner(
		ReplayByteStream &replay_stream,
		std::chrono::milliseconds step_interval, bool playback_mode
	) noexcept;

	TuiRunner(const TuiRunner &) = delete;
	TuiRunner &operator=(const TuiRunner &) = delete;

	void notifyUpdate() noexcept;
	int run();

private:
	ReplayByteStream &_replay_stream;
	std::chrono::milliseconds _step_interval;
	bool _playback_mode = false;
	ftxui::ScreenInteractive _screen;
};

int runTui(
	ReplayByteStream &replay_stream, std::chrono::milliseconds step_interval,
	bool playback_mode
);

} // namespace cr

#endif
