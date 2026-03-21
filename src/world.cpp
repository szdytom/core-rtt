#include "corertt/world.h"
#include "corertt/entity.h"
#include "corertt/runtime.h"
#include "corertt/xoroshiro.h"
#include <algorithm>
#include <cpptrace/basic.hpp>
#include <iostream>
#include <queue>
#include <random>
#include <set>
#include <tuple>
#include <vector>

namespace cr {

constexpr int base_capture_threshold = 8;

namespace {
std::pair<pos_t, pos_t> getDirectionOffset(Direction dir) noexcept {
	switch (dir) {
	case Direction::Up:
		return {0, -1};
	case Direction::Down:
		return {0, 1};
	case Direction::Left:
		return {-1, 0};
	case Direction::Right:
		return {1, 0};
	}
	return {0, 0};
}
} // namespace

Player::Player(int id) noexcept
	: id(id), base_energy(0), base_capture_counter(0) {
	units.fill(nullptr);
}

void Player::step(World &world) noexcept {
	// Shuffle outgoing messages to prevent reliance on message order?
	// In thoery we should do so to maximize the security of the messaging
	// system, but I'm quiet interested in seeing some creative uses of message
	// ordering by players, so let's keep it for now.
	// Note: the current message order is the order of manufacture actions,
	// which is deterministic and known to players anyway.

	// std::shuffle(
	// 	outgoing_messages.begin(), outgoing_messages.end(),
	// 	Xoroshiro128PP::globalInstance()
	// );
	incoming_messages = std::move(outgoing_messages);
	outgoing_messages.clear();

	if (!_base_machine) {
		_base_machine = std::make_unique<RVMachine>(base_elf, RUNTIME_OPTION);
		_base_ecall_ctx.bind(*_base_machine);
	}

	_base_ecall_ctx.simulate(world, *this, nullptr, *_base_machine);
	if (_base_ecall_ctx.stop_reason != StoppedReason::NOT_STOPPED) {
		_base_machine.reset();
	}
}

World::World(Tilemap tilemap) noexcept
	: tick(0), _tilemap(std::move(tilemap)), _players{Player(1), Player(2)} {
	// Initialize player base positions based on tilemap
	// Find out top-left corner of each base and assign to players
	bool base_found[2] = {false, false};
	for (int x = 0; x < width(); ++x) {
		for (int y = 0; y < height(); ++y) {
			auto tile = _tilemap.tileOf(x, y);
			if (!tile.is_base) {
				continue;
			}

			int pid = tile.side - 1;
			if (base_found[pid]) {
				continue; // Already found base for this player
			}

			_players[pid].base_x = x;
			_players[pid].base_y = y;
			base_found[pid] = true;
		}
	}

#ifndef NDEBUG
	if (!base_found[0] || !base_found[1]) {
		std::cerr << "Error: failed to find base for both players in tilemap\n";
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
#endif

	// Init each base with 3 units
	for (auto &player : _players) {
		for (int i = 1; i <= 3; ++i) {
			_spawnUnitAtBase(player.id, i);
		}
	}
}

void World::step() noexcept {
	tick += 1;
	_processBulletMovement();
	_processUnitMovement();
	_checkBaseCaptureCondition();
	_collectEnergy();

	for (auto &player : _players) {
		player.step(*this);
	}

	for (auto &unit : _units) {
		unit->step(*this, _players[unit->player_id - 1]);
	}
}

void World::_processBulletMovement() noexcept {
	for (int i = 0; i < 5; ++i) {
		for (auto &bullet : _bullets) {
			if (bullet->damage == 0) {
				continue;
			}

			_moveBulletOneStep(*bullet);
		}

		if (i == 0) {
			_commitPendingAttacks();
		}
	}

	// Remove destroyed bullets
	auto is_destroyed = [](const auto &b) {
		return b->damage == 0;
	};

	_bullets.erase(
		std::remove_if(_bullets.begin(), _bullets.end(), is_destroyed),
		_bullets.end()
	);
}

void World::_moveBulletOneStep(Bullet &bullet) noexcept {
	auto [dx, dy] = getDirectionOffset(bullet.direction);
	pos_t new_x = bullet.x + dx;
	pos_t new_y = bullet.y + dy;

	// Check bounds
	if (!isInBounds(new_x, new_y)) {
		bullet.damage = 0;
		return;
	}

	// Check obstacle
	auto &new_tile = _tilemap.tileOf(new_x, new_y);
	if (new_tile.terrain == Tile::OBSTACLE) {
		bullet.damage = 0;
		return;
	}

	// Update old tile
	auto &old_tile = _tilemap.tileOf(bullet.x, bullet.y);
	old_tile.unsetOccupant();

	_handleBulletCollision(bullet, new_x, new_y);
	if (bullet.damage == 0) {
		return;
	}

	bullet.x = new_x;
	bullet.y = new_y;
	new_tile.bullet_ptr = &bullet;
	new_tile.occupied_state = Tile::OCCUPIED_BY_BULLET;
}

void World::_handleBulletCollision(
	Bullet &bullet, pos_t new_x, pos_t new_y
) noexcept {
	auto &tile = _tilemap.tileOf(new_x, new_y);

	switch (tile.occupied_state) {
	case Tile::NOT_OCCUPIED:
		break;

	case Tile::OCCUPIED_BY_UNIT:
		if (auto unit = tile.unit_ptr) {
			if (bullet.damage > unit->health) {
				bullet.damage -= unit->health;

				unit->health = 0;
				tile.unsetOccupant();
				_players[unit->player_id - 1].units[unit->id - 1] = nullptr;
			} else {
				unit->health -= bullet.damage;
				bullet.damage = 0;
			}
		}
		break;

	case Tile::OCCUPIED_BY_BULLET:
		if (auto other = tile.bullet_ptr) {
			if (bullet.damage > other->damage) {
				bullet.damage -= other->damage;
				other->damage = 0;
				tile.bullet_ptr = nullptr;
				tile.occupied_state = Tile::NOT_OCCUPIED;
			} else {
				other->damage -= bullet.damage;
				bullet.damage = 0;
				if (other->damage == 0) {
					tile.bullet_ptr = nullptr;
					tile.occupied_state = Tile::NOT_OCCUPIED;
				}
			}
		}
		break;
	}
}

void World::_processUnitMovement() noexcept {
	auto is_destroyed = [](const auto &u) {
		return u->health == 0;
	};

	_units.erase(
		std::remove_if(_units.begin(), _units.end(), is_destroyed), _units.end()
	);

	// The rules:
	// 1. If the target tile is out of bounds or an obstacle, the movement is
	// cancelled.
	// 2. If two units try to move into the same tile, both movements are
	// cancelled.
	// 3. If a unit tries to move into a tile occupied by another unit, the
	// movement will be cancelled if
	// 		a. the occupant has no pending move, or
	// 		b. the occupant's pending move is moving against the incoming unit
	// (e.g. A moves right into B, and B moves left into A), or
	// 		c. the occupant's pending move is cancelled
	//
	// The algorithm:
	// 1. Collect all intended moves, immediately cancelling those violating
	// rule #1, rule #3a, and rule #3b.
	// 2. Detect collisions by counting how many units intend to move into each
	// tile. Cancel moves that violate rule #2.
	// 3. Build a graph of remaining valid moves. If a unit A intends to move
	// into a tile occupied by B, add an edge A -> B.
	// 4. Backpropagate cancellations: if a unit's move is cancelled, cancel all
	// moves that depend on it (i.e. all outgoing edges from it).
	// 5. Execute remaining valid moves.

	auto n = static_cast<int>(_units.size());

	std::vector<pos_t> tgt_x(n), tgt_y(n);
	std::vector<bool> active(n, false);

	for (int i = 0; i < n; ++i) {
		auto &unit = *_units[i];
		tgt_x[i] = unit.x;
		tgt_y[i] = unit.y;
		if (!unit.pending_move.has_value()) {
			continue;
		}

		// Step 1: compute target positions, cancel moves violating rule #1
		// (out of bounds, obstacle, or water)
		auto [dx, dy] = getDirectionOffset(*unit.pending_move);
		pos_t nx = unit.x + dx;
		pos_t ny = unit.y + dy;

		if (!isInBounds(nx, ny)) {
			continue;
		}

		auto tile = _tilemap.tileOf(nx, ny);
		auto terrain = tile.terrain;
		if (terrain == Tile::OBSTACLE || terrain == Tile::WATER) {
			continue;
		}

		// Step 2: rule #3a & #3b
		switch (tile.occupied_state) {
		case Tile::NOT_OCCUPIED:
			break;

		case Tile::OCCUPIED_BY_BULLET:
			continue; // Can't move into a tile occupied by a bullet

		case Tile::OCCUPIED_BY_UNIT:
			if (auto occ = tile.unit_ptr) {
				if (!occ->pending_move.has_value()) {
					continue; // rule #3a
				}

				auto [odx, ody] = getDirectionOffset(*occ->pending_move);
				pos_t onx = occ->x + odx;
				pos_t ony = occ->y + ody;

				if (onx == unit.x && ony == unit.y) {
					continue; // rule #3b
				}
			}
			break;
		}

		tgt_x[i] = nx;
		tgt_y[i] = ny;
		active[i] = true;
	}

	// Step 3: build dependency graph and cancel rule #3a
	// If unit i targets a tile occupied by unit j:
	//   - j is not actively moving → cancel i (rule #3a)
	//   - j is actively moving → add dependency edge: blocked_by[j] includes i
	std::vector<std::vector<int>> blocked_by(n);

	for (int i = 0; i < n; ++i) {
		if (!active[i]) {
			continue;
		}

		auto &tile = _tilemap.tileOf(tgt_x[i], tgt_y[i]);
		if (tile.occupied_state != Tile::OCCUPIED_BY_UNIT) {
			continue;
		}

		// OPTIMIZEME: faster way to find occupant index than linear scan
		// e.g. sort units by memory address and binary search, or maintain a
		// map from unit pointer to index
		int occ = -1;
		for (int j = 0; j < n; ++j) {
			if (_units[j].get() == tile.unit_ptr) {
				occ = j;
				break;
			}
		}

		if (occ == -1) {
			// Shouldn't happen
#ifndef NDEBUG
			std::cerr << "Error: occupant not found or inactive\n";
			cpptrace::generate_trace().print(std::cerr);
			std::abort();
#endif
			active[i] = false;
			continue;
		}

		blocked_by[occ].push_back(i);
	}

	// Step 4: cancel rule #2 (multiple units targeting the same tile)
	std::set<std::tuple<pos_t, pos_t>> targeted_tiles;
	std::set<std::tuple<pos_t, pos_t>> cancelled_tiles;
	for (int i = 0; i < n; ++i) {
		if (!active[i]) {
			continue;
		}

		auto tile_pos = std::make_tuple(tgt_x[i], tgt_y[i]);
		if (targeted_tiles.count(tile_pos) > 0) {
			cancelled_tiles.insert(tile_pos);
		} else {
			targeted_tiles.insert(tile_pos);
		}
	}

	for (int i = 0; i < n; ++i) {
		if (!active[i]) {
			continue;
		}

		auto tile_pos = std::make_tuple(tgt_x[i], tgt_y[i]);
		if (cancelled_tiles.count(tile_pos) > 0) {
			active[i] = false;
		}
	}

	// Step 5: BFS propagation — if a unit's move is cancelled,
	// cancel all moves that depend on it walking away.
	std::queue<int> cancel_queue;
	for (int i = 0; i < n; ++i) {
		if (!active[i]) {
			cancel_queue.push(i);
		}
	}

	while (!cancel_queue.empty()) {
		int curr = cancel_queue.front();
		cancel_queue.pop();
		for (int dep : blocked_by[curr]) {
			if (active[dep]) {
				active[dep] = false;
				cancel_queue.push(dep);
			}
		}
	}

	// Step 6: execute remaining valid moves
	for (int i = 0; i < n; ++i) {
		_units[i]->pending_move.reset();
	}

	// First pass: unset old tile occupancy for all moving units
	for (int i = 0; i < n; ++i) {
		if (!active[i]) {
			continue;
		}
		_tilemap.tileOf(_units[i]->x, _units[i]->y).unsetOccupant();
	}

	// Second pass: update positions and set new tile occupancy
	for (int i = 0; i < n; ++i) {
		if (!active[i]) {
			continue;
		}

		auto &unit = *_units[i];
		unit.x = tgt_x[i];
		unit.y = tgt_y[i];

		auto &new_tile = _tilemap.tileOf(unit.x, unit.y);
		new_tile.unit_ptr = &unit;
		new_tile.occupied_state = Tile::OCCUPIED_BY_UNIT;
	}
}

void World::_checkBaseCaptureCondition() noexcept {
	for (int pid : {1, 2}) {
		auto &player = _players[pid - 1];
		int enemy_id = 3 - pid;

		int own_units_in_base = 0;
		int enemy_units_in_base = 0;

		for (const auto &unit : _units) {
			if (unit->health == 0) {
				continue;
			}
			if (isInBase(unit->x, unit->y, pid)) {
				if (unit->player_id == pid) {
					own_units_in_base += 1;
				} else {
					enemy_units_in_base += 1;
				}
			}
		}

		if (enemy_units_in_base > own_units_in_base) {
			++player.base_capture_counter;
		} else {
			player.base_capture_counter = 0;
		}
	}

	if (_players[0].base_capture_counter == base_capture_threshold
	    && _players[1].base_capture_counter == base_capture_threshold) {
		// Simultaneous capture, match continues
		_players[0].base_capture_counter -= 1;
		_players[1].base_capture_counter -= 1;
	}
}

void World::_collectEnergy() noexcept {
	for (auto &unit : _units) {
		if (unit->health == 0) {
			continue;
		}

		// Base energy collection: 1 per turn
		energy_t gained = 1;

		// Resource zone bonus: 25 per turn
		auto tile = _tilemap.tileOf(unit->x, unit->y);
		if (tile.is_resource) {
			gained += 25;
		}

		unit->energy = std::min<energy_t>(
			unit->energy + gained, unit->maxCapacity()
		);
	}
}

ActionResult World::manufactureUnit(
	std::uint8_t player_id, std::uint8_t new_unit_id
) noexcept {
	auto &player = _players[player_id - 1];

	constexpr energy_t manufacture_cost = 500;
	if (player.base_energy < manufacture_cost) {
		return ActionResult::INSUFFICIENT_ENERGY;
	}

	if (new_unit_id < 1 || new_unit_id > max_units) {
		return ActionResult::INVALID_ID;
	}

	if (player.units[new_unit_id - 1] != nullptr
	    && player.units[new_unit_id - 1]->health > 0) {
		return ActionResult::INVALID_ID;
	}

	// All checks passed, create the unit at a random empty tile in the base
	player.base_energy -= manufacture_cost;

	return ActionResult::OK;
}

void World::_spawnUnitAtBase(std::uint8_t player_id, std::uint8_t unit_id) {
	auto &player = _players[player_id - 1];
	pos_t base_size = _tilemap.baseSize();

	std::vector<std::tuple<pos_t, pos_t>> empty_tiles;

	for (pos_t dx = 0; dx < base_size; ++dx) {
		for (pos_t dy = 0; dy < base_size; ++dy) {
			pos_t x = player.base_x + dx;
			pos_t y = player.base_y + dy;

			auto &tile = _tilemap.tileOf(x, y);
			if (tile.occupied_state == Tile::NOT_OCCUPIED) {
				empty_tiles.emplace_back(x, y);
			}
		}
	}

	if (empty_tiles.empty()) {
		// Insufficient capacity, unit can't be spawned.
		throw std::runtime_error("Failed to spawn unit at base: no empty tile");
	}

	// Randomly select an empty tile to spawn the unit
	auto &rng = Xoroshiro128PP::globalInstance();
	std::uniform_int_distribution<size_t> dist(0, empty_tiles.size() - 1);
	auto [spawn_x, spawn_y] = empty_tiles[dist(rng)];
	_spawnUnit(player_id, unit_id, spawn_x, spawn_y);
}

void World::_spawnUnit(
	std::uint8_t player_id, std::uint8_t unit_id, pos_t x, pos_t y
) noexcept {
	auto new_unit = std::make_unique<Unit>(unit_id, player_id, x, y);

	auto &tile = _tilemap.tileOf(x, y);
	tile.unit_ptr = new_unit.get();
	tile.occupied_state = Tile::OCCUPIED_BY_UNIT;

	_players[player_id - 1].units[unit_id - 1] = new_unit.get();
	_units.push_back(std::move(new_unit));
}

ActionResult World::repairUnit(
	std::uint8_t player_id, std::uint8_t unit_id
) noexcept {
	auto &player = _players[player_id - 1];

	// Validate unit ID
	if (unit_id < 1 || unit_id > max_units) {
		return ActionResult::INVALID_ID;
	}

	Unit *unit = player.units[unit_id - 1];

	if (!unit || unit->health == 0) {
		return ActionResult::INVALID_ID;
	}

	if (!isInBase(unit->x, unit->y, player_id)) {
		return ActionResult::INVALID_UNIT;
	}

	// Calculate repair cost
	health_t missing_health = Unit::MAX_HEALTH - unit->health;
	if (missing_health == 0) {
		return ActionResult::INVALID_UNIT; // No repair needed
	}

	energy_t repair_cost = 2 * missing_health;
	if (player.base_energy < repair_cost) {
		return ActionResult::INSUFFICIENT_ENERGY;
	}

	// Perform repair
	player.base_energy -= repair_cost;
	unit->health = Unit::MAX_HEALTH;
	return ActionResult::OK;
}

ActionResult World::upgradeUnit(
	std::uint8_t player_id, std::uint8_t unit_id, UpgradeType type
) noexcept {
	auto &player = _players[player_id - 1];

	if (unit_id < 1 || unit_id > max_units) {
		return ActionResult::INVALID_ID;
	}

	Unit *unit = player.units[unit_id - 1];

	if (!unit || unit->health == 0) {
		return ActionResult::INVALID_ID;
	}

	if (!isInBase(unit->x, unit->y, player_id)) {
		return ActionResult::INVALID_UNIT;
	}

	energy_t cost;
	bool already_upgraded = false;

	switch (type) {
	case UpgradeType::Capacity:
		cost = 400;
		already_upgraded = unit->upgrades.capacity;
		break;
	case UpgradeType::Vision:
		cost = 1000;
		already_upgraded = unit->upgrades.vision;
		break;
	case UpgradeType::Damage:
		cost = 600;
		already_upgraded = unit->upgrades.damage;
		break;
	default:
		return ActionResult::OUT_OF_RANGE;
	}

	if (already_upgraded) {
		return ActionResult::INVALID_UNIT;
	}

	if (player.base_energy < cost) {
		return ActionResult::INSUFFICIENT_ENERGY;
	}

	player.base_energy -= cost;

	switch (type) {
	case UpgradeType::Capacity:
		unit->upgrades.capacity = true;
		break;
	case UpgradeType::Vision:
		unit->upgrades.vision = true;
		break;
	case UpgradeType::Damage:
		unit->upgrades.damage = true;
		break;
	}

	return ActionResult::OK;
}

bool World::isInBounds(pos_t x, pos_t y) const noexcept {
	return x >= 0 && x < _tilemap.width() && y >= 0 && y < _tilemap.height();
}

bool World::isInBase(pos_t x, pos_t y, int player_id) const noexcept {
	const auto &player = _players[player_id - 1];
	pos_t base_size = _tilemap.baseSize();
	return x >= player.base_x && x < player.base_x + base_size
		&& y >= player.base_y && y < player.base_y + base_size;
}

Unit *World::findUnit(std::uint8_t unit_id, std::uint8_t player_id) noexcept {
	if (player_id < 1 || player_id > 2 || unit_id < 1 || unit_id > max_units) {
		return nullptr;
	}
	return _players[player_id - 1].units[unit_id - 1];
}

void World::_commitPendingAttacks() noexcept {
	for (auto &unit : _units) {
		if (unit->health == 0 || unit->pending_attack.damage == 0) {
			continue;
		}

		// Calculate actual bullet damage based on upgrade
		auto bullet_damage = unit->pending_attack.damage;

		auto [dx, dy] = getDirectionOffset(unit->pending_attack.direction);
		pos_t target_x = unit->x + dx;
		pos_t target_y = unit->y + dy;
		auto &tile = _tilemap.tileOf(target_x, target_y);
		if (tile.terrain == Tile::OBSTACLE) {
			continue; // Can't shoot into obstacle
		}

		auto bullet = std::make_unique<Bullet>(
			target_x, target_y, unit->pending_attack.direction, bullet_damage,
			unit->player_id
		);

		_handleBulletCollision(*bullet, target_x, target_y);
		if (bullet->damage == 0) {
			continue; // Bullet destroyed immediately, no need to add to world
		}

		tile.bullet_ptr = bullet.get();
		tile.occupied_state = Tile::OCCUPIED_BY_BULLET;

		_bullets.push_back(std::move(bullet));
		unit->attack_cooldown = 3;
		unit->pending_attack.damage = 0;
	}
}

void World::setPlayerProgram(
	int player_id, std::vector<std::uint8_t> base_elf,
	std::vector<std::uint8_t> unit_elf
) noexcept {
#ifndef NDEBUG
	if (player_id < 1 || player_id > 2) {
		std::cerr << "Error: invalid player_id in setPlayerProgram\n";
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
#endif

	auto &player = _players[player_id - 1];
	player.base_elf = std::move(base_elf);
	player.unit_elf = std::move(unit_elf);
}

void World::appendRuntimeLog(
	std::uint8_t player_id, std::uint8_t unit_id, std::string message
) {
	_runtime_logs.push_back(RuntimeLogEntry{
		.tick = currentTick(),
		.player_id = player_id,
		.unit_id = unit_id,
		.message = std::move(message),
	});

	while (_runtime_logs.size() > max_runtime_logs) {
		_runtime_logs.pop_front();
	}
}

std::vector<RuntimeLogEntry>
World::runtimeLogsSnapshot(std::size_t max_entries) const {
	if (_runtime_logs.empty() || max_entries == 0) {
		return {};
	}

	const std::size_t count = std::min(max_entries, _runtime_logs.size());
	const auto begin = _runtime_logs.end() - static_cast<std::ptrdiff_t>(count);
	return std::vector<RuntimeLogEntry>(begin, _runtime_logs.end());
}

} // namespace cr
