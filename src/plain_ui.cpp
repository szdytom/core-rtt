#include "corertt/plain_ui.h"
#include "corertt/replay.h"
#include <format>

namespace cr {

namespace {

std::string_view toString(ReplayTermination termination) noexcept {
	switch (termination) {
	case ReplayTermination::Completed:
		return "completed";
	case ReplayTermination::Aborted:
		return "aborted";
	}
	return "aborted";
}

} // namespace

PlainUi::PlainUi(std::ostream &output, std::ostream &error) noexcept
	: _output(output), _error(error) {}

void PlainUi::start() {
	_output << "UI mode: plain\n";
}

int PlainUi::wait() {
	return 0;
}

void PlainUi::publishHeader(const ReplayHeader &header) {
	_output << std::format(
		"H v={} map={}x{} base={}\n", header.version, header.tilemap.width,
		header.tilemap.height, header.tilemap.base_size
	);
}

void PlainUi::publishTick(const ReplayTickFrame &tick) {
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

void PlainUi::publishEnd(const ReplayEndMarker &end_marker) {
	_output << std::format(
		"E term={} winner={}\n", toString(end_marker.termination),
		end_marker.winner_player_id
	);
	_output.flush();
}

void PlainUi::publishError(const std::string &message) {
	_error << std::format("ERR {}\n", message);
	_error.flush();
}

bool PlainUi::shouldStop() const noexcept {
	return _stop_requested.load(std::memory_order_relaxed);
}

void PlainUi::requestStop() noexcept {
	_stop_requested.store(true, std::memory_order_relaxed);
}

} // namespace cr
