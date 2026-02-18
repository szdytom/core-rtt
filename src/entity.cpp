#include "corertt/entity.h"
#include "corertt/runtime.h"
#include "corertt/world.h"
#include <queue>
#include <tuple>

namespace cr {

Upgrades::Upgrades() noexcept: capacity(false), vision(false), damage(false) {}

Upgrades::operator std::uint8_t() const noexcept {
	std::uint8_t res = 0;
	if (capacity) {
		res |= 0b001;
	}
	if (vision) {
		res |= 0b010;
	}
	if (damage) {
		res |= 0b100;
	}
	return res;
}

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
	return upgrades.vision ? 9 : 5; // 9x9 vs 5x5 area
}

bool Unit::canAttack() const noexcept {
	return attack_cooldown == 0;
}

void Unit::step(World &world, Player &player) noexcept {
	if (health == 0) {
		return;
	}

	if (!_machine) {
		_machine = std::make_unique<RVMachine>(player.unit_elf, RUNTIME_OPTION);
		_ecall_ctx.bind(*_machine);
	}

	if (attack_cooldown > 0) {
		attack_cooldown -= 1;
	}

	// Compute vision cache using BFS
	const int range = visionRange();
	const int total_tiles = range * range;

	_ecall_ctx.visible_tiles.assign(total_tiles, false);
	std::queue<std::tuple<pos_t, pos_t>> queue;
	std::vector<bool> visited(total_tiles, false);
	const pos_t topleft_x = x - range / 2, topleft_y = y - range / 2;
	queue.emplace(range / 2, range / 2);

	const auto &tilemap = world.tilemap();

	while (!queue.empty()) {
		auto [cx, cy] = queue.front();
		queue.pop();

		int idx = cy * range + cx;
		if (visited[idx]) {
			continue;
		}
		visited[idx] = true;

		pos_t tx = topleft_x + cx;
		pos_t ty = topleft_y + cy;
		if (!world.isInBounds(tx, ty)) {
			continue;
		}

		const auto tile = tilemap.tileOf(tx, ty);
		if (tile.terrain == Tile::OBSTACLE) {
			continue;
		}
		_ecall_ctx.visible_tiles[idx] = true;

		const pos_t dx[] = {0, 0, -1, 1};
		const pos_t dy[] = {-1, 1, 0, 0};

		for (int dir = 0; dir < 4; ++dir) {
			pos_t nx = cx + dx[dir];
			pos_t ny = cy + dy[dir];
			int idx = ny * range + nx;

			if (nx < 0 || nx >= range || ny < 0 || ny >= range
			    || visited[idx]) {
				continue;
			}
			queue.emplace(nx, ny);
		}
	}

	// Against pointer invalidation issues, set up the ecall context on each
	// step instead of setting it up once and assuming it remains valid.
	// not a good practice, but we keep it until we have a better solution for
	// pointer management in the runtime.
	_ecall_ctx.player = &player;
	_ecall_ctx.unit = this;
	_ecall_ctx.world = &world;
	_machine->set_userdata(&_ecall_ctx);

	constexpr int max_instructions_per_turn = 100'000;
	if (_machine->simulate<false>(max_instructions_per_turn)) {
		_machine.reset();
	}
}

Bullet::Bullet(
	pos_t x, pos_t y, Direction dir, std::uint16_t dmg, std::uint8_t pid
) noexcept
	: x(x), y(y), direction(dir), damage(dmg), player_id(pid) {}

} // namespace cr