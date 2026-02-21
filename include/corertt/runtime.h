#ifndef CORERTT_RUNTIME_H
#define CORERTT_RUNTIME_H

#include "corertt/xoroshiro.h"
#include <cstddef>
#include <libriscv/common.hpp>
#include <libriscv/machine.hpp>
#include <vector>

namespace cr {

class Unit;
class Player;
class World;

enum class ActionResult : std::int32_t {
	OK = 0,
	INVALID_POINTER = -1,
	INVALID_UNIT = -2,
	INSUFFICIENT_ENERGY = -3,
	ON_COOLDOWN = -4,
	CAPACITY_FULL = -5,
	INVALID_ID = -6,
	OUT_OF_RANGE = -7,
	UNSUPPORTED_RUNTIME = -8,
};

enum class StoppedReason : std::int32_t {
	NOT_STOPPED = 0,
	ABORTED,
	PAGE_PROTECTION_FAULT,
	ALIGNMENT_FAULT,
	ILLEGAL_INSTRUCTION,
	OUT_OF_MEMORY,
	UNKOWN_EXCEPTION
};

struct ECallNumber {
	// Debugging calls
	static constexpr std::size_t ABORT = 0x00;
	static constexpr std::size_t LOG = 0x01;

	// Common calls
	static constexpr std::size_t META = 0x0F;
	static constexpr std::size_t TURN = 0x10;
	static constexpr std::size_t DEV_INFO = 0x11;
	static constexpr std::size_t READ_SENSOR = 0x12;
	static constexpr std::size_t RECV_MSG = 0x13;
	static constexpr std::size_t SEND_MSG = 0x14;
	static constexpr std::size_t POS = 0x15;

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

struct ECallInvokedFlags {
	bool send_msg : 1 = false;
	bool manufact : 1 = false;
	bool repair : 1 = false;
	bool upgrade : 1 = false;
	bool move : 1 = false;
	bool deposit_withdraw : 1 = false;
};

struct RuntimeECallContext {
	World *world;         // borrowed pointer
	Player *player;       // borrowed pointer
	Unit *unit = nullptr; // borrowed, nullptr for base calls
	StoppedReason stop_reason;
	int msg_idx;
	Xoroshiro128PP rng;
	ECallInvokedFlags flags;

	// Vision cache for units: stores which tiles are visible
	// Size is (2*visionRange+1)^2 for units, indexed by [dy * (2*range+1) + dx]
	std::vector<bool> visible_tiles;

	void simulate(
		World &world, Player &player, Unit *unit, RVMachine &machine
	) noexcept;
	void bind(RVMachine &machine) noexcept;
};

} // namespace cr

#endif // CORERTT_RUNTIME_H
