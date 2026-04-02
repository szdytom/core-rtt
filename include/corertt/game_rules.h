#ifndef CORERTT_GAME_RULES_H
#define CORERTT_GAME_RULES_H

#include <cstdint>

namespace cr {

using pos_t = std::int16_t;
using energy_t = std::uint16_t;
using health_t = std::uint8_t;

struct GameRules {
	std::uint8_t width = 64;
	std::uint8_t height = 64;
	std::uint8_t base_size = 5;
	health_t unit_health = 100;
	std::uint8_t natural_energy_rate = 1;
	std::uint8_t resource_zone_energy_rate = 25;
	std::uint8_t attack_cooldown = 3;
	std::uint8_t capture_turn_threshold = 8;
	std::uint8_t vision_lv1 = 5;
	std::uint8_t vision_lv2 = 9;
	energy_t capacity_lv1 = 200;
	energy_t capacity_lv2 = 1000;
	energy_t capacity_upgrade_cost = 400;
	energy_t vision_upgrade_cost = 1000;
	energy_t damage_upgrade_cost = 600;
	energy_t manufact_cost = 500;

	bool validate() const noexcept;
};

} // namespace cr

#endif // CORERTT_GAME_RULES_H
