#ifndef CORERTT_RUNTIME_H
#define CORERTT_RUNTIME_H

#include <cstddef>
#include <libriscv/common.hpp>
#include <libriscv/machine.hpp>
#include <vector>

namespace cr {

class Unit;
class Player;
class World;

enum class ActionResult : std::int32_t {
	Success = 0,
	PageFault = -1,
	InvalidUnit = -2,
	InsufficientEnergy = -3,
	OnCooldown = -4,
	CapacityFull = -5,
	InvalidID = -6,
	OutOfBounds = -7,
};

struct ECallNumber {
	// Debugging calls
	static constexpr std::size_t LOG = 0x01;

	// Common calls
	static constexpr std::size_t TURN = 0x10;
	static constexpr std::size_t DEV_INFO = 0x11;
	static constexpr std::size_t READ_SENSOR = 0x12;
	static constexpr std::size_t RECV_MSG = 0x13;
	static constexpr std::size_t SEND_MSG = 0x14;

	// Base calls
	static constexpr std::size_t MANUFACT = 0x20;
	static constexpr std::size_t REPAIR = 0x21;
	static constexpr std::size_t UPGRADE = 0x22;

	// Unit action calls
	static constexpr std::size_t FIRE = 0x30;
	static constexpr std::size_t MOVE = 0x31;
	static constexpr std::size_t DEPOSIT = 0x32;
	static constexpr std::size_t WITHDRAW = 0x33;

	// Untility calls
	static constexpr std::size_t HEAP_BASE_ID = 0x40;
	static constexpr std::size_t MALLOC = 0x40;
	static constexpr std::size_t CALLOC = 0x41;
	static constexpr std::size_t REALLOC = 0x42;
	static constexpr std::size_t FREE = 0x43;
	static constexpr std::size_t MEMINFO = 0x44;
	static constexpr std::size_t RAND = 0x4F;
};

using RVMachine = riscv::Machine<riscv::RISCV32>;

inline const riscv::MachineOptions<riscv::RISCV32> RUNTIME_OPTION = {
	.memory_max = 128 * 1024, // 128KB memory
	.stack_size = 4 * 1024,   // 4KB stack
};

struct RuntimeECallContext {
	World *world;         // borrowed pointer
	Player *player;       // borrowed pointer
	Unit *unit = nullptr; // borrowed, nullptr for base calls

	// Vision cache for units: stores which tiles are visible
	// Size is (2*visionRange+1)^2 for units, indexed by [dy * (2*range+1) + dx]
	std::vector<bool> visible_tiles;

	void simulate(RVMachine &machine) noexcept;
	void bind(RVMachine &machine) noexcept;
};

} // namespace cr

#endif // CORERTT_RUNTIME_H
