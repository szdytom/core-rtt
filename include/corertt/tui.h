#ifndef CORERTT_TUI_H
#define CORERTT_TUI_H

#include "corertt/replay.h"
#include <ftxui/component/screen_interactive.hpp>
#include <string>

namespace cr {

struct SynchronizedReplayProgress {
	std::mutex mutex;
	ReplayProgress progress;
	std::string last_error;
};

class TuiRunner {
public:
	explicit TuiRunner(SynchronizedReplayProgress &replay) noexcept;

	TuiRunner(const TuiRunner &) = delete;
	TuiRunner &operator=(const TuiRunner &) = delete;

	void notifyUpdate() noexcept;
	int run();

private:
	SynchronizedReplayProgress &_replay;
	ftxui::ScreenInteractive _screen;
};

} // namespace cr

#endif
