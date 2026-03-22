#ifndef CORERTT_TUI_H
#define CORERTT_TUI_H

#include "corertt/replay.h"
#include <chrono>

namespace cr {

int runTui(
	ReplayByteStream &replay_stream, std::chrono::milliseconds step_interval
);

} // namespace cr

#endif
