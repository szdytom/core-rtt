#ifndef CORERTT_TUI_H
#define CORERTT_TUI_H

#include <chrono>

namespace cr {

class World;

int runTui(World &world, std::chrono::milliseconds step_interval);

} // namespace cr

#endif
