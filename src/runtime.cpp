#include "corertt/runtime.h"
#include "corertt/world.h"
#include "cpptrace/cpptrace.hpp"
#include <bit>
#include <cstdlib>
#include <iostream>
#include <libriscv/common.hpp>
#include <memory>
#include <print>
#include <string_view>

namespace cr {

namespace {

#ifndef NDEBUG
RuntimeECallContext *check_userdata(RVMachine &machine) {
	auto ctx = machine.get_userdata<RuntimeECallContext>();
	if (!ctx) {
		std::cerr << "ECALL error: userdata is null\n";
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}

	if (ctx->world == nullptr) {
		std::cerr << "ECALL error: world pointer is null\n";
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
	return ctx;
}
#else
RuntimeECallContext *check_userdata(RVMachine &machine) {
	// Do nothing in release mode.
	return machine.get_userdata<RuntimeECallContext>();
}
#endif

void debug_memory_allocation(RVMachine &machine) {
	std::println(
		"Flat arena: {}\n"
		"  Arena size: {}\n"
		"  Initial rodata end: {}\n"
		"  Initial read boundary: {}\n"
		"  Initial write boundary: {}",
		machine.memory.uses_flat_memory_arena() ? "Enabled" : "Disabled",
		machine.memory.memory_arena_size(), machine.memory.initial_rodata_end(),
		machine.memory.memory_arena_read_boundary(),
		machine.memory.memory_arena_write_boundary()
	);

	auto &pages = machine.memory.pages();
	std::println(std::cerr, "Currently allocated {} pages:", pages.size());
	for (auto &[addr, page] : pages) {
		std::println(std::cerr, "  {:#x}", addr << 12);
	}

	std::println(
		std::cerr, "Active pages: {}, Owned Active pages: {}",
		machine.memory.pages_active(), machine.memory.owned_pages_active()
	);
}

void ecall_log(RVMachine &machine) {
	auto [ptr, len] = machine.sysargs<RVMachine::address_t, int>();
	try {
		std::unique_ptr<char[]> buffer = std::make_unique<char[]>(len);
		machine.copy_from_guest(buffer.get(), ptr, len);
		std::string_view view(buffer.get(), buffer.get() + len);
		std::println("ECALL LOG: {}", view);
		machine.set_result(ActionResult::Success);
	} catch (...) {
		machine.set_result(ActionResult::PageFault);
		return;
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
		machine.set_result(ActionResult::PageFault);
	}
}

} // namespace

void RuntimeECallContext::bind(RVMachine &machine) noexcept {
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
	machine.install_syscall_handler(ECallNumber::LOG, ecall_log);
	machine.install_syscall_handler(ECallNumber::TURN, ecall_turn);
	machine.install_syscall_handler(ECallNumber::DEV_INFO, ecall_dev_info);
	machine.install_syscall_handler(
		ECallNumber::READ_SENSOR, ecall_read_sensor
	);
	machine.install_syscall_handler(
		riscv::SYSCALL_EBREAK, debug_memory_allocation
	);

	auto heap_addr = machine.memory.heap_address();
	constexpr size_t heap_size = 128 * 1024;
	machine.setup_native_heap(ECallNumber::HEAP_BASE_ID, heap_addr, heap_size);
}

} // namespace cr
