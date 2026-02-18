#ifndef CORERTT_WORLD_H
#define CORERTT_WORLD_H

#include "corertt/entity.h"
#include "corertt/runtime.h"
#include "corertt/tilemap.h"
#include <array>
#include <memory>
#include <vector>

namespace cr {

constexpr int max_units = 15;

enum class UpgradeType : std::uint8_t {
	Capacity = 0,
	Vision = 1,
	Damage = 2,
};

class World;

struct Player {
	int id;               // 1 or 2
	pos_t base_x, base_y; // top-left corner of the base
	energy_t base_energy;
	std::array<Unit *, max_units> units; // 0-indexed, non-owned, nullable
	int base_capture_counter; // consecutive turns enemy controls base

	std::vector<std::uint8_t> unit_elf; // ELF binary for unit
	std::vector<std::uint8_t> base_elf; // ELF binary for base

	Player() noexcept = default;
	Player(int id) noexcept;

	void step(World &) noexcept;

private:
	std::unique_ptr<RVMachine> _base_machine;
	RuntimeECallContext _base_ecall_ctx;
};

class World {
public:
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

private:
	std::uint32_t tick;
	Tilemap _tilemap;
	Player _players[2];
	std::vector<std::unique_ptr<Unit>> _units;
	std::vector<std::unique_ptr<Bullet>> _bullets;

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
