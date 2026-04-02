#include "corelib.h"

int main(void) {
	struct GameInfo info;
	assert(meta(&info) == EC_OK);

	assert(info.map_width == 20);
	assert(info.map_height == 18);
	assert(info.base_size == 3);
	assert(info.unit_health == 123);
	assert(info.natural_energy_rate == 7);
	assert(info.resource_zone_energy_rate == 31);
	assert(info.attack_cooldown == 4);
	assert(info.capture_turn_threshold == 11);
	assert(info.vision_lv1 == 6);
	assert(info.vision_lv2 == 10);
	assert(info.capacity_lv1 == 345);
	assert(info.capacity_lv2 == 1200);
	assert(info.capacity_upgrade_cost == 450);
	assert(info.vision_upgrade_cost == 1100);
	assert(info.damage_upgrade_cost == 650);
	assert(info.manufact_cost == 777);

	log_str("[PASS] meta reports all overridden gamerules.");
	while (true) {}
}