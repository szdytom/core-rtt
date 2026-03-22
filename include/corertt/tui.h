#ifndef CORERTT_TUI_H
#define CORERTT_TUI_H

#include <chrono>
#include <functional>

namespace cr {

class World;

using TickCallback = std::function<void(World &)>;

int runTui(
	World &world, std::chrono::milliseconds step_interval,
	TickCallback tick_callback = {}
);

} // namespace cr

#endif
