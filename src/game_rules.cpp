#include "corertt/game_rules.h"

namespace cr {

namespace {

template<typename T>
bool isInRange(T value, T min_value, T max_value) noexcept {
	return value >= min_value && value <= max_value;
}

} // namespace

bool GameRules::validate() const noexcept {
	return isInRange<std::uint8_t>(width, 4, 255)
		&& isInRange<std::uint8_t>(height, 4, 255)
		&& isInRange<std::uint8_t>(base_size, 2, 8)
		&& isInRange<health_t>(unit_health, 1, 255)
		&& isInRange<std::uint32_t>(natural_energy_rate, 1, 255)
		&& isInRange<energy_t>(resource_zone_energy_rate, 1, 255)
		&& isInRange<std::uint8_t>(attack_cooldown, 1, 255)
		&& isInRange<std::uint8_t>(capture_turn_threshold, 1, 255)
		&& isInRange<std::uint8_t>(vision_lv1, 1, 255)
		&& isInRange<std::uint8_t>(vision_lv2, 1, 255)
		&& isInRange<energy_t>(capacity_lv1, 1, 65535)
		&& isInRange<energy_t>(capacity_lv2, 1, 65535)
		&& isInRange<energy_t>(capacity_upgrade_cost, 1, 65535)
		&& isInRange<energy_t>(vision_upgrade_cost, 1, 65535)
		&& isInRange<energy_t>(damage_upgrade_cost, 1, 65535)
		&& isInRange<energy_t>(manufact_cost, 1, 65535)
		&& capacity_lv2 >= capacity_lv1 && vision_lv2 >= vision_lv1;
}

} // namespace cr
