#include "../include/corertt/entity.h"

namespace cr {

Upgrades::Upgrades() noexcept: capacity(false), vision(false), damage(false) {}

Unit::Unit(std::uint8_t id, std::uint8_t player_id, pos_t x, pos_t y) noexcept
	: id(id)
	, player_id(player_id)
	, x(x)
	, y(y)
	, health(100)
	, energy(0)
	, attack_cooldown(0)
	, pending_attack{.damage = 0} {}

energy_t Unit::maxCapacity() const noexcept {
	return upgrades.capacity ? 1000 : 200;
}

int Unit::visionRange() const noexcept {
	return upgrades.vision ? 4 : 2; // 9x9 vs 5x5 area
}

bool Unit::canAttack() const noexcept {
	return attack_cooldown == 0;
}

Bullet::Bullet(
	pos_t x, pos_t y, Direction dir, std::uint16_t dmg, std::uint8_t pid
) noexcept
	: x(x), y(y), direction(dir), damage(dmg), player_id(pid) {}

} // namespace cr