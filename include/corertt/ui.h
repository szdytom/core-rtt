#ifndef CORERTT_UI_H
#define CORERTT_UI_H

#include "corertt/replay.h"
#include <string>

namespace cr {

enum class UiMode {
	Tui,
	Plain,
};

class IUi {
public:
	virtual ~IUi() = default;

	virtual void start() = 0;
	virtual int wait() = 0;

	virtual void publishHeader(const ReplayHeader &header) = 0;
	virtual void publishTick(const ReplayTickFrame &tick) = 0;
	virtual void publishEnd(const ReplayEndMarker &end_marker) = 0;
	virtual void publishError(const std::string &message) = 0;

	virtual bool shouldStop() const noexcept = 0;
	virtual void requestStop() noexcept = 0;
};

} // namespace cr

#endif
