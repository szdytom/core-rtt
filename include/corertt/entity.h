#ifndef CORE_RTT_ENTITY_H
#define CORE_RTT_ENTITY_H

#include <cstdint>
#include <optional>

namespace cr {

using pos_t = std::int16_t;
using energy_t = std::uint16_t;
using health_t = std::uint8_t;

enum class Direction : std::uint8_t {
	Up = 0,
	Down = 1,
	Left = 2,
	Right = 3,
};

struct Upgrades {
	bool capacity : 1;
	bool vision : 1;
	bool damage : 1;

	Upgrades() noexcept;
};

struct Unit {
	static constexpr health_t MAX_HEALTH = 100;

	std::uint8_t id;        // 1-15
	std::uint8_t player_id; // 1 or 2
	pos_t x;
	pos_t y;
	health_t health;              // 0-100
	energy_t energy;              // carried energy
	std::uint8_t attack_cooldown; // turns remaining until can attack again
	Upgrades upgrades;

	std::optional<Direction> pending_move;
	struct {
		health_t damage;
		Direction direction;
	} pending_attack;

	Unit() noexcept = default;
	Unit(std::uint8_t id, std::uint8_t player_id, pos_t x, pos_t y) noexcept;

	energy_t maxCapacity() const noexcept;
	int visionRange() const noexcept;
	bool canAttack() const noexcept;
};

struct Bullet {
	pos_t x;
	pos_t y;
	Direction direction;
	std::uint16_t damage;   // damage value
	std::uint8_t player_id; // 1 or 2, owner of the bullet

	Bullet() noexcept = default;
	Bullet(
		pos_t x, pos_t y, Direction dir, std::uint16_t dmg, std::uint8_t pid
	) noexcept;
};

} // namespace cr

#endif
