#include "corertt/runtime.h"
#include "corertt/entity.h"
#include "corertt/fail_fast.h"
#include "corertt/replay.h"
#include "corertt/world.h"
#include "corertt/xoroshiro.h"
#include <bit>
#include <libriscv/common.hpp>
#include <libriscv/types.hpp>
#include <memory>
#include <print>

namespace cr {

namespace {

RuntimeECallContext *check_userdata(RVMachine &machine) {
	auto ctx = machine.get_userdata<RuntimeECallContext>();
	CR_FAIL_FAST_ASSERT_LIGHT(ctx != nullptr, "userdata is null");
	CR_FAIL_FAST_ASSERT_LIGHT(ctx->world != nullptr, "world pointer is null");
	return ctx;
}

void ecall_ebreak(RVMachine &machine) {}

void ecall_abort(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	ctx->stop_reason = StoppedReason::ABORTED;
	machine.stop();
}

void ecall_log(RVMachine &machine) {
	constexpr int LENGTH_LIMIT = 512;

	auto [ptr, len] = machine.sysargs<RVMachine::address_t, int>();
	if (len < 1 || len > LENGTH_LIMIT) {
		machine.set_result(ActionResult::OUT_OF_RANGE);
		return;
	}

	auto ctx = check_userdata(machine);
	if (len > ctx->log_quota) {
		machine.set_result(ActionResult::ON_COOLDOWN);
		return;
	}
	ctx->log_quota -= len;

	try {
		auto entry = ReplayLogEntry::customLog(
			ctx->world->currentTick(), ctx->player->id,
			(ctx->unit ? ctx->unit->id : 0), static_cast<std::size_t>(len)
		);
		machine.copy_from_guest(entry.payload.data(), ptr, len);
		ctx->world->appendLog(std::move(entry));
		machine.penalize(len);
		machine.set_result(ActionResult::OK);
	} catch (...) {
		machine.set_result(ActionResult::INVALID_POINTER);
		return;
	}
}

void ecall_meta(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	struct GameInfo {
		std::uint8_t map_width;
		std::uint8_t map_height;
		std::uint8_t base_size;
		std::uint8_t reserved[13];
	};
	static_assert(sizeof(GameInfo) == 16, "GameInfo must be 16 bytes");
	GameInfo info{};
	info.map_width = static_cast<std::uint8_t>(ctx->world->width());
	info.map_height = static_cast<std::uint8_t>(ctx->world->height());
	info.base_size = static_cast<std::uint8_t>(
		ctx->world->tilemap().baseSize()
	);
	try {
		auto [ptr] = machine.sysargs<RVMachine::address_t>();
		machine.copy_to_guest(ptr, &info, sizeof(info));
		machine.set_result(ActionResult::OK);
	} catch (...) {
		machine.set_result(ActionResult::INVALID_POINTER);
	}
}

void ecall_turn(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	machine.set_result(ctx->world->currentTick());
}

void ecall_dev_info(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	struct {
		std::uint8_t id : 5;
		std::uint8_t upgrades : 3;
		std::uint8_t health;
		std::uint16_t energy;
	} res;
	static_assert(sizeof(res) == 4, "DeviceInfo struct must be 4 bytes");

	if (auto unit = ctx->unit) {
		res.id = unit->id;
		res.upgrades = unit->upgrades;
		res.health = unit->health;
		res.energy = unit->energy;
	} else {
		res.id = 0;
		res.upgrades = 0;
		res.health = 0;
		res.energy = ctx->player->base_energy;
	}
	machine.set_result(std::bit_cast<std::uint32_t>(res));
}

void ecall_read_sensor(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	const auto &tilemap = ctx->world->tilemap();

	struct SensorData {
		bool visible : 1;
		std::uint8_t terrain : 2;
		bool is_base : 1;
		bool is_resource : 1;
		bool has_unit : 1;
		bool has_bullet : 1;
		bool alliance_unit : 1;
	};
	static_assert(sizeof(SensorData) == 1, "SensorData must be 1 byte");

	int range;
	pos_t topleft_x, topleft_y;

	if (ctx->unit) {
		auto unit = ctx->unit;
		range = unit->visionRange();
		topleft_x = unit->x - range / 2;
		topleft_y = unit->y - range / 2;
	} else {
		range = tilemap.baseSize();
		topleft_x = ctx->player->base_x;
		topleft_y = ctx->player->base_y;
	}

	const int total_tiles = range * range;

	try {
		std::vector<SensorData> sensor_data(total_tiles);

		for (int dy = 0; dy < range; ++dy) {
			for (int dx = 0; dx < range; ++dx) {
				int idx = dy * range + dx;
				auto &data = sensor_data[idx];
				pos_t tx = topleft_x + dx;
				pos_t ty = topleft_y + dy;

				if (!ctx->world->isInBounds(tx, ty)) {
					data.visible = false;
					continue;
				}

				if (ctx->unit) {
					data.visible = ctx->visible_tiles[idx];
					if (!data.visible) {
						continue;
					}
				} else {
					data.visible = true;
				}

				const auto tile = tilemap.tileOf(tx, ty);
				data.terrain = tile.terrain;
				data.is_base = tile.is_base;
				data.is_resource = tile.is_resource;
				data.has_unit = (tile.occupied_state == Tile::OCCUPIED_BY_UNIT);
				data.has_bullet
					= (tile.occupied_state == Tile::OCCUPIED_BY_BULLET);

				if (data.has_unit && tile.unit_ptr) {
					data.alliance_unit
						= (tile.unit_ptr->player_id == ctx->player->id);
				} else {
					data.alliance_unit = false;
				}
			}
		}

		auto [data_ptr] = machine.sysargs<RVMachine::address_t>();
		machine.copy_to_guest(
			data_ptr, sensor_data.data(), total_tiles * sizeof(SensorData)
		);

		machine.penalize(total_tiles);
		machine.set_result(total_tiles);
	} catch (...) {
		machine.set_result(ActionResult::INVALID_POINTER);
	}
}

void ecall_recv_msg(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	const auto &messages = ctx->player->incoming_messages;
	if (ctx->msg_idx >= messages.size()) {
		// Read 0 bytes
		machine.set_result(0);
		return;
	}

	const auto &msg = messages[ctx->msg_idx++];
	auto [ptr, len] = machine.sysargs<RVMachine::address_t, int>();
	if (len < 0) {
		machine.set_result(ActionResult::OUT_OF_RANGE);
		return;
	}

	if (len == 0) {
		// Skip message
		machine.set_result(0);
		return;
	}

	int copy_len = std::min<int>(len, msg.length);
	try {
		machine.copy_to_guest(ptr, msg.data.data(), copy_len);
		machine.set_result(copy_len);
	} catch (...) {
		machine.set_result(ActionResult::INVALID_POINTER);
	}
}

void ecall_send_msg(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	auto [ptr, len] = machine.sysargs<RVMachine::address_t, int>();
	if (len < 0 || len > MessagePacket::packet_size) {
		machine.set_result(ActionResult::OUT_OF_RANGE);
		return;
	}

	if (ctx->flags.send_msg) {
		machine.set_result(ActionResult::ON_COOLDOWN);
		return;
	}
	ctx->flags.send_msg = true;

	try {
		MessagePacket msg;
		msg.length = len;
		machine.copy_from_guest(msg.data.data(), ptr, len);
		// COCURRRENT LOCKME
		// lock_player(ctx->player)
		ctx->player->outgoing_messages.push_back(std::move(msg));
		// unlock_player(ctx->player)
		machine.set_result(ActionResult::OK);
	} catch (...) {
		machine.set_result(ActionResult::INVALID_POINTER);
	}
}

void ecall_manufact(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	if (ctx->unit) {
		machine.set_result(ActionResult::UNSUPPORTED_RUNTIME);
		return;
	}

	if (ctx->flags.manufact) {
		machine.set_result(ActionResult::ON_COOLDOWN);
		return;
	}

	auto [unit_id] = machine.sysargs<int>();
	// COCURRENT LOCKME
	// lock_world(ctx->world); lock_player(ctx->player);
	auto res = ctx->world->manufactureUnit(ctx->player->id, unit_id);
	// unlock_world(ctx->world); unlock_player(ctx->player);
	if (res == ActionResult::OK) {
		ctx->flags.manufact = true;
	}
	machine.set_result(res);
}

void ecall_repair(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	if (ctx->unit) {
		machine.set_result(ActionResult::UNSUPPORTED_RUNTIME);
		return;
	}

	if (ctx->flags.repair) {
		machine.set_result(ActionResult::ON_COOLDOWN);
		return;
	}

	auto [unit_id] = machine.sysargs<int>();
	auto res = ctx->world->repairUnit(ctx->player->id, unit_id);
	if (res == ActionResult::OK) {
		ctx->flags.repair = true;
	}
	machine.set_result(res);
}

void ecall_upgrade(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	if (ctx->unit) {
		machine.set_result(ActionResult::UNSUPPORTED_RUNTIME);
		return;
	}

	if (ctx->flags.upgrade) {
		machine.set_result(ActionResult::ON_COOLDOWN);
		return;
	}

	auto [unit_id, upgrade_id] = machine.sysargs<int, int>();
	auto upgrade_type = static_cast<UpgradeType>(upgrade_id);
	auto res = ctx->world->upgradeUnit(ctx->player->id, unit_id, upgrade_type);
	if (res == ActionResult::OK) {
		ctx->flags.upgrade = true;
	}
	machine.set_result(res);
}

void ecall_fire(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	if (!ctx->unit) {
		machine.set_result(ActionResult::UNSUPPORTED_RUNTIME);
		return;
	}

	if (!ctx->unit->canAttack()) {
		machine.set_result(ActionResult::ON_COOLDOWN);
		return;
	}

	auto [dir_id, power] = machine.sysargs<int, int>();
	if (dir_id < 0 || dir_id > 3 || power < 1) {
		machine.set_result(ActionResult::OUT_OF_RANGE);
		return;
	}

	auto dir = static_cast<Direction>(dir_id);
	if (power > ctx->unit->energy) {
		machine.set_result(ActionResult::INSUFFICIENT_ENERGY);
		return;
	}
	auto damage = power * (ctx->unit->upgrades.damage ? 2 : 1);
	if (damage > std::numeric_limits<health_t>::max()) {
		machine.set_result(ActionResult::OUT_OF_RANGE);
		return;
	}

	ctx->unit->energy -= power;
	ctx->unit->pending_attack = {
		.damage = static_cast<health_t>(damage),
		.direction = dir,
	};
	ctx->unit->attack_cooldown = 3;
	machine.set_result(ActionResult::OK);
}

void ecall_move(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	if (!ctx->unit) {
		machine.set_result(ActionResult::UNSUPPORTED_RUNTIME);
		return;
	}

	if (ctx->flags.move) {
		machine.set_result(ActionResult::ON_COOLDOWN);
		return;
	}

	auto [dir_id] = machine.sysargs<int>();
	if (dir_id < 0 || dir_id > 3) {
		ctx->unit->pending_move = std::nullopt;
		machine.set_result(ActionResult::OUT_OF_RANGE);
		return;
	}

	auto dir = static_cast<Direction>(dir_id);
	ctx->unit->pending_move = dir;
	ctx->flags.move = true;
	machine.set_result(ActionResult::OK);
}

void ecall_deposit(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	if (!ctx->unit) {
		machine.set_result(ActionResult::UNSUPPORTED_RUNTIME);
		return;
	}

	if (ctx->flags.deposit_withdraw) {
		machine.set_result(ActionResult::ON_COOLDOWN);
		return;
	}

	auto [amount] = machine.sysargs<int>();
	if (amount < 1) {
		machine.set_result(ActionResult::OUT_OF_RANGE);
		return;
	}

	if (amount > ctx->unit->energy) {
		machine.set_result(ActionResult::INSUFFICIENT_ENERGY);
		return;
	}

	if (!ctx->world->isInBase(ctx->unit->x, ctx->unit->y, ctx->player->id)) {
		machine.set_result(ActionResult::INVALID_UNIT);
		return;
	}

	ctx->unit->energy -= amount;
	// COCURRENT LOCKME
	// lock_player(ctx->player);
	auto value = ctx->player->base_energy + amount;
	if (value > std::numeric_limits<energy_t>::max()) {
		value = std::numeric_limits<energy_t>::max();
	}
	ctx->player->base_energy = value;
	// unlock_player(ctx->player);
	ctx->flags.deposit_withdraw = true;
	machine.set_result(ActionResult::OK);
}

void ecall_withdraw(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	if (!ctx->unit) {
		machine.set_result(ActionResult::UNSUPPORTED_RUNTIME);
		return;
	}

	if (ctx->flags.deposit_withdraw) {
		machine.set_result(ActionResult::ON_COOLDOWN);
		return;
	}

	auto [amount] = machine.sysargs<int>();
	if (amount < 1) {
		machine.set_result(ActionResult::OUT_OF_RANGE);
		return;
	}

	if (amount > ctx->player->base_energy) {
		machine.set_result(ActionResult::INSUFFICIENT_ENERGY);
		return;
	}

	if (!ctx->world->isInBase(ctx->unit->x, ctx->unit->y, ctx->player->id)) {
		machine.set_result(ActionResult::INVALID_UNIT);
		return;
	}

	auto value = ctx->unit->energy + amount;
	if (value > ctx->unit->maxCapacity()) {
		value = ctx->unit->maxCapacity();
	}
	ctx->unit->energy = value;
	// COCURRENT LOCKME
	// lock_player(ctx->player);
	ctx->player->base_energy -= amount;
	// unlock_player(ctx->player);
	ctx->flags.deposit_withdraw = true;
	machine.set_result(ActionResult::OK);
}

void ecall_pos(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	std::uint8_t x, y;
	if (ctx->unit) {
		x = static_cast<std::uint8_t>(ctx->unit->x);
		y = static_cast<std::uint8_t>(ctx->unit->y);
	} else {
		x = static_cast<std::uint8_t>(ctx->player->base_x);
		y = static_cast<std::uint8_t>(ctx->player->base_y);
	}
	// Pack as {x, y, 0, 0} into a uint32_t (little-endian).
	std::uint32_t result = static_cast<std::uint32_t>(x)
		| (static_cast<std::uint32_t>(y) << 8);
	machine.set_result(result);
}

void ecall_rand(RVMachine &machine) {
	auto ctx = check_userdata(machine);
	constexpr std::uint32_t u32_mask = 0xFF'FF'FF'FF;
	std::uint32_t rand_val = ctx->rng.next() & u32_mask;
	machine.set_result(rand_val);
}

} // namespace

namespace {

StoppedReason machineExceptionToStoppedReason(
	const riscv::MachineException &e
) noexcept {
	switch (e.type()) {
	case riscv::INVALID_PROGRAM:
		return StoppedReason::INVALID_PROGRAM;

	case riscv::ILLEGAL_OPERATION:
	case riscv::ILLEGAL_OPCODE:
	case riscv::UNIMPLEMENTED_INSTRUCTION:
	case riscv::UNIMPLEMENTED_INSTRUCTION_LENGTH:
		return StoppedReason::ILLEGAL_INSTRUCTION;

	case riscv::OUT_OF_MEMORY:
		return StoppedReason::OUT_OF_MEMORY;

	case riscv::PROTECTION_FAULT:
	case riscv::EXECUTION_SPACE_PROTECTION_FAULT:
		return StoppedReason::PAGE_PROTECTION_FAULT;

	case riscv::MISALIGNED_INSTRUCTION:
	case riscv::INVALID_ALIGNMENT:
		return StoppedReason::ALIGNMENT_FAULT;

	default:
		return StoppedReason::UNKNOWN_EXCEPTION;
	}
}

} // namespace

std::expected<std::unique_ptr<RVMachine>, StoppedReason> createMachineFromELF(
	std::span<const std::uint8_t> elf_binary
) noexcept {
	const riscv::MachineOptions<riscv::RISCV32> RUNTIME_OPTION = {
		.memory_max = 128 * 1024, // 128KB memory
		.stack_size = 4 * 1024,   // 4KB stack
	};

	try {
		return std::make_unique<RVMachine>(elf_binary, RUNTIME_OPTION);
	} catch (const riscv::MachineException &e) {
		return std::unexpected(machineExceptionToStoppedReason(e));
	} catch (...) {
		return std::unexpected(StoppedReason::UNKNOWN_EXCEPTION);
	}
}

void RuntimeECallContext::bind(RVMachine &machine) noexcept {
	rng = Xoroshiro128PP{Seed::deviceRandom()};
	stop_reason = StoppedReason::NOT_STOPPED;
	machine.set_userdata(this);
	machine.cpu.reg(riscv::REG_SP) = 0;
	machine.set_printer([](const auto &, const auto, auto) {
		// Do nothing.
	});
	machine.on_unhandled_syscall = [](auto &m, auto num) {
		// Do nothing. not even returning -1, to avoid potential issues with
		// programs that rely on the return value of unimplemented syscalls.
	};

	// Register ecall handlers
	machine.install_syscall_handler(ECallNumber::ABORT, ecall_abort);
	machine.install_syscall_handler(ECallNumber::LOG, ecall_log);
	machine.install_syscall_handler(ECallNumber::META, ecall_meta);
	machine.install_syscall_handler(ECallNumber::TURN, ecall_turn);
	machine.install_syscall_handler(ECallNumber::DEV_INFO, ecall_dev_info);
	machine.install_syscall_handler(
		ECallNumber::READ_SENSOR, ecall_read_sensor
	);
	machine.install_syscall_handler(ECallNumber::RECV_MSG, ecall_recv_msg);
	machine.install_syscall_handler(ECallNumber::SEND_MSG, ecall_send_msg);
	machine.install_syscall_handler(ECallNumber::POS, ecall_pos);
	machine.install_syscall_handler(ECallNumber::RAND, ecall_rand);
	machine.install_syscall_handler(ECallNumber::MANUFACT, ecall_manufact);
	machine.install_syscall_handler(ECallNumber::REPAIR, ecall_repair);
	machine.install_syscall_handler(ECallNumber::UPGRADE, ecall_upgrade);
	machine.install_syscall_handler(ECallNumber::FIRE, ecall_fire);
	machine.install_syscall_handler(ECallNumber::MOVE, ecall_move);
	machine.install_syscall_handler(ECallNumber::DEPOSIT, ecall_deposit);
	machine.install_syscall_handler(ECallNumber::WITHDRAW, ecall_withdraw);

	machine.install_syscall_handler(riscv::SYSCALL_EBREAK, ecall_ebreak);

	auto heap_addr = machine.memory.heap_address();
	constexpr size_t heap_size = 128 * 1024;
	machine.setup_native_heap(ECallNumber::HEAP_BASE_ID, heap_addr, heap_size);
}

void RuntimeECallContext::simulate(
	World &world, Player &player, Unit *unit, RVMachine &machine
) noexcept {
	constexpr int MAX_LOG_QUOTA = 4096;

	msg_idx = 0;
	flags = {};
	this->world = &world;
	this->player = &player;
	this->unit = unit;
	log_quota += 32;
	if (log_quota > MAX_LOG_QUOTA) {
		log_quota = MAX_LOG_QUOTA;
	}

	machine.set_userdata(this);

	constexpr int max_cycles = 100'000;
	try {
		machine.simulate<false>(max_cycles);
	} catch (const riscv::MachineException &e) {
		stop_reason = machineExceptionToStoppedReason(e);
	} catch (...) {
		stop_reason = StoppedReason::UNKNOWN_EXCEPTION;
	}
}

} // namespace cr
