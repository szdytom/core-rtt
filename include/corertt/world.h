#ifndef CORERTT_WORLD_H
#define CORERTT_WORLD_H

#include "corertt/entity.h"
#include "corertt/replay.h"
#include "corertt/runtime.h"
#include "corertt/tilemap.h"
#include <array>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

namespace cr {

constexpr int max_units = 15;

enum class WorldTerminationReason : std::uint8_t {
	Ongoing = 0,
	Victory = 1,
	RuleDraw = 2,
};

struct TurnEvents {
	bool unit_left_base : 1;
	bool unit_entered_resource_zone : 1;
	bool unit_upgraded_or_repaired_at_base : 1;
	bool unit_manufactured : 1;
	bool unit_destroyed : 1;
	bool base_entered_capture_condition : 1;
};

struct GameRules {
	int width = 64;
	int height = 64;
	int base_size = 5;
	health_t unit_health = 100;
	std::uint32_t natural_energy_rate = 1;
	energy_t resource_zone_energy_rate = 25;
	std::uint8_t attack_cooldown = 3;
	energy_t capacity_lv1 = 200;
	energy_t capacity_lv2 = 1000;
	std::uint8_t vision_lv1 = 5;
	std::uint8_t vision_lv2 = 9;
	energy_t capacity_upgrade_cost = 400;
	energy_t vision_upgrade_cost = 1000;
	energy_t damage_upgrade_cost = 600;
	energy_t manufact_cost = 500;
	int capture_turn_threshold = 8;

	bool validate() const noexcept;
};

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

	World(Tilemap tilemap, GameRules rules) noexcept;

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
		return _termination_reason != WorldTerminationReason::Ongoing;
	}

	WorldTerminationReason terminationReason() const noexcept {
		return _termination_reason;
	}

	std::uint8_t winnerPlayerId() const noexcept {
		return _winner_player_id;
	}

	std::uint8_t capturedPlayerId() const noexcept {
		return _winner_player_id == 0 ? 0 : 3 - _winner_player_id;
	}

	const Tilemap &tilemap() const noexcept {
		return _tilemap;
	}

	const GameRules &rules() const noexcept {
		return _rules;
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

	const TurnEvents &turnEvents() const noexcept {
		return _turn_events;
	}

	void markRuleDraw() noexcept;

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
	GameRules _rules;
	Player _players[2];
	std::vector<std::unique_ptr<Unit>> _units;
	std::vector<std::unique_ptr<Bullet>> _bullets;
	std::vector<ReplayLogEntry> _runtime_logs;
	TurnEvents _turn_events{};
	WorldTerminationReason _termination_reason;
	std::uint8_t _winner_player_id;

	// Helper methods
	void _markVictory(std::uint8_t winner_player_id) noexcept;
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
