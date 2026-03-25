#ifndef CORERTT_PLAIN_UI_H
#define CORERTT_PLAIN_UI_H

#include "corertt/ui.h"
#include <atomic>
#include <iosfwd>

namespace cr {

class PlainUIRunner final : public UIRunner {
public:
	PlainUIRunner(std::ostream &output, std::ostream &error) noexcept;

	void start() override;
	int wait() override;

	void publishHeader(const ReplayHeader &header) override;
	void publishTick(const ReplayTickFrame &tick) override;
	void publishEnd(const ReplayEndMarker &end_marker) override;
	void publishError(const std::string &message) override;

	bool shouldStop() const noexcept override;
	void requestStop() noexcept override;

private:
	std::ostream &_output;
	std::ostream &_error;
	std::atomic<bool> _stop_requested = false;
};

} // namespace cr

#endif
