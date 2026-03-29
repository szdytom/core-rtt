#ifndef CORERTT_WORLD_H
#define CORERTT_WORLD_H

#include "corertt/entity.h"
#include "corertt/replay.h"
#include "corertt/runtime.h"
#include "corertt/tilemap.h"
#include <array>
#include <memory>
#include <ranges>
#include <vector>

namespace cr {

constexpr int max_units = 15;

enum class UpgradeType : std::uint8_t {
	Capacity = 0,
	Vision = 1,
	Damage = 2,
};

class World;

struct MessagePacket {
	static constexpr std::size_t packet_size = 512;
	std::uint8_t length;
	std::array<std::uint8_t, packet_size> data;
};

struct Player {
	std::uint16_t id;     // 1 or 2
	pos_t base_x, base_y; // top-left corner of the base
	energy_t base_energy;
	std::array<Unit *, max_units> units; // 0-indexed, non-owned, nullable
	int base_capture_counter; // consecutive turns enemy controls base

	bool unit_elf_crash_flag;
	bool base_elf_crash_flag;
	std::vector<std::uint8_t> unit_elf; // ELF binary for unit
	std::vector<std::uint8_t> base_elf; // ELF binary for base

	std::vector<MessagePacket> incoming_messages;
	std::vector<MessagePacket> outgoing_messages;

	Player() noexcept = default;
	Player(std::uint16_t id) noexcept;

	void step(World &) noexcept;

private:
	std::unique_ptr<RVMachine> _base_machine;
	RuntimeECallContext _base_ecall_ctx;
};

class World {
public:
	using RuntimeLogConstIterator = std::vector<ReplayLogEntry>::const_iterator;

	World(Tilemap tilemap) noexcept;

	int width() const noexcept {
		return _tilemap.width();
	}

	int height() const noexcept {
		return _tilemap.height();
	}

	std::uint32_t currentTick() const noexcept {
		return tick;
	}

	bool gameOver() const noexcept {
		return _winner_player_id != 0;
	}

	std::uint8_t winnerPlayerId() const noexcept {
		return _winner_player_id;
	}

	std::uint8_t capturedPlayerId() const noexcept {
		return 3 - _winner_player_id;
	}

	const Tilemap &tilemap() const noexcept {
		return _tilemap;
	}

	const Player &player(int id) const noexcept {
		return _players[id - 1];
	}

	// Simulate one game tick
	void step() noexcept;

	// Base actions
	ActionResult manufactureUnit(
		std::uint8_t player_id, std::uint8_t new_unit_id
	) noexcept;
	ActionResult repairUnit(
		std::uint8_t player_id, std::uint8_t unit_id
	) noexcept;
	ActionResult upgradeUnit(
		std::uint8_t player_id, std::uint8_t unit_id, UpgradeType type
	) noexcept;

	Unit *findUnit(std::uint8_t unit_id, std::uint8_t player_id) noexcept;
	bool isInBounds(pos_t x, pos_t y) const noexcept;
	bool isInBase(pos_t x, pos_t y, int player_id) const noexcept;
	bool canMoveTo(pos_t x, pos_t y) const noexcept;
	void setPlayerProgram(
		int player_id, std::vector<std::uint8_t> base_elf,
		std::vector<std::uint8_t> unit_elf
	) noexcept;
	void appendLog(ReplayLogEntry entry);
	std::vector<ReplayLogEntry> takeTickLogs() noexcept;

	auto runtimeLogsBegin() const noexcept {
		return _runtime_logs.cbegin();
	}

	auto runtimeLogsEnd() const noexcept {
		return _runtime_logs.cend();
	}

	auto runtimeLogs() const noexcept {
		return std::ranges::subrange(runtimeLogsBegin(), runtimeLogsEnd());
	}

private:
	friend class ReplayTickFrame;

	std::uint32_t tick;
	Tilemap _tilemap;
	Player _players[2];
	std::vector<std::unique_ptr<Unit>> _units;
	std::vector<std::unique_ptr<Bullet>> _bullets;
	std::vector<ReplayLogEntry> _runtime_logs;
	std::uint8_t _winner_player_id;

	// Helper methods
	void _processBulletMovement() noexcept;
	void _processUnitMovement() noexcept;
	void _checkBaseCaptureCondition() noexcept;
	void _collectEnergy() noexcept;

	void _handleBulletCollision(
		Bullet &bullet, pos_t new_x, pos_t new_y
	) noexcept;
	void _moveBulletOneStep(Bullet &bullet) noexcept;
	void _commitPendingAttacks() noexcept;

	void _spawnUnitAtBase(std::uint8_t player_id, std::uint8_t unit_id);
	void _spawnUnit(
		std::uint8_t player_id, std::uint8_t unit_id, pos_t x, pos_t y
	) noexcept;
};

} // namespace cr

#endif
