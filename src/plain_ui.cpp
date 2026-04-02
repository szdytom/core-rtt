#include "corertt/plain_ui.h"
#include "corertt/replay.h"
#include <format>

namespace cr {

namespace {

std::string_view toString(ReplayTermination termination) noexcept {
	switch (termination) {
	case ReplayTermination::Completed:
		return "completed";
	case ReplayTermination::RuleDraw:
		return "rule-draw";
	case ReplayTermination::Aborted:
		return "aborted";
	}
	return "aborted";
}

} // namespace

PlainUIRunner::PlainUIRunner(std::ostream &output, std::ostream &error) noexcept
	: _output(output), _error(error) {}

void PlainUIRunner::start() {
	_output << "UI mode: plain\n";
}

int PlainUIRunner::wait() {
	return 0;
}

void PlainUIRunner::publishHeader(const ReplayHeader &header) {
	_output << std::format(
		"H v={} map={}x{} base={}\n", header.version, header.tilemap.width,
		header.tilemap.height, header.tilemap.base_size
	);
	_output << std::format(
		"G width={} height={} base={} unit_health={} natural_energy_rate={} "
		"resource_zone_energy_rate={} attack_cooldown={} "
		"capture_turn_threshold={} vision_lv1={} vision_lv2={} capacity_lv1={} "
		"capacity_lv2={} capacity_upgrade_cost={} vision_upgrade_cost={} "
		"damage_upgrade_cost={} manufact_cost={}\n",
		header.game_rules.width, header.game_rules.height,
		header.game_rules.base_size, header.game_rules.unit_health,
		header.game_rules.natural_energy_rate,
		header.game_rules.resource_zone_energy_rate,
		header.game_rules.attack_cooldown,
		header.game_rules.capture_turn_threshold, header.game_rules.vision_lv1,
		header.game_rules.vision_lv2, header.game_rules.capacity_lv1,
		header.game_rules.capacity_lv2, header.game_rules.capacity_upgrade_cost,
		header.game_rules.vision_upgrade_cost,
		header.game_rules.damage_upgrade_cost, header.game_rules.manufact_cost
	);
}

void PlainUIRunner::publishTick(const ReplayTickFrame &tick) {
	_output << std::format(
		"T t={} p1(e={},c={}) p2(e={},c={}) u={} b={} logs={}\n", tick.tick,
		tick.players[0].base_energy, tick.players[0].base_capture_counter,
		tick.players[1].base_energy, tick.players[1].base_capture_counter,
		tick.units.size(), tick.bullets.size(), tick.logs.size()
	);

	for (const auto &unit : tick.units) {
		_output << std::format(
			"U id={} p={} ({},{}) hp={} en={} cd={} up={}\n", unit.id,
			unit.player_id, unit.x, unit.y, unit.health, unit.energy,
			unit.attack_cooldown, unit.upgrades
		);
	}

	for (std::size_t index = 0; index < tick.bullets.size(); ++index) {
		const auto &bullet = tick.bullets[index];
		_output << std::format(
			"B #{} p={} ({},{}) dir={} dmg={}\n", index, bullet.player_id,
			bullet.x, bullet.y, bullet.direction, bullet.damage
		);
	}

	for (const auto &entry : tick.logs) {
		for (auto &line : FormatReplayLogEntryLines(entry)) {
			_output << std::format("L {}\n", line);
		}
	}

	_output.flush();
}

void PlainUIRunner::publishEnd(const ReplayEndMarker &end_marker) {
	_output << std::format(
		"E term={} winner={}\n", toString(end_marker.termination),
		end_marker.winner_player_id
	);
	_output.flush();
}

void PlainUIRunner::publishError(const std::string &message) {
	_error << std::format("ERR {}\n", message);
	_error.flush();
}

bool PlainUIRunner::shouldStop() const noexcept {
	return false;
}

void PlainUIRunner::requestStop() noexcept {}

} // namespace cr
