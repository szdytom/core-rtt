#ifndef CORERTT_TUI_H
#define CORERTT_TUI_H

#include "corertt/replay.h"
#include "corertt/ui.h"
#include <atomic>
#include <ftxui/component/screen_interactive.hpp>
#include <mutex>
#include <string>
#include <thread>

namespace cr {

struct TuiSynchronizedReplayProgress {
	std::mutex mutex;
	ReplayProgress progress;
	std::string last_error;
};

class TuiRunner final : public UIRunner {
public:
	TuiRunner() noexcept;
	~TuiRunner() override;

	TuiRunner(const TuiRunner &) = delete;
	TuiRunner &operator=(const TuiRunner &) = delete;

	void start() override;
	int wait() override;

	void publishHeader(const ReplayHeader &header) override;
	void publishTick(const ReplayTickFrame &tick) override;
	void publishEnd(const ReplayEndMarker &end_marker) override;
	void publishError(const std::string &message) override;

	bool shouldStop() const noexcept override;
	void requestStop() noexcept override;

private:
	void notifyUpdate() noexcept;
	void runUIThread(std::stop_token stop_token);

	TuiSynchronizedReplayProgress _replay;
	std::atomic<bool> _stop_requested = false;
	std::jthread _ui_thread;
	ftxui::ScreenInteractive _screen;
};

} // namespace cr

#endif
