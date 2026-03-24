#include "corelib.h"

#define MAX_MAP_SIZE 64
#define MAX_CELLS (MAX_MAP_SIZE * MAX_MAP_SIZE)
#define MSG_MAX_LEN 512

enum {
	PACKET_MAP_DELTA = 1,
};

enum {
	CELL_EMPTY = 0,
	CELL_WATER = 1,
	CELL_OBSTACLE = 2,
};

enum {
	MODE_EXPLORE = 0,
	MODE_TO_RESOURCE = 1,
	MODE_TO_BASE = 2,
	MODE_SCOUT = 3,
};

static struct GameInfo game_info;
static struct SensorData sensor_data[81];

struct TileDelta {
	uint8_t x;
	uint8_t y;
	uint8_t bits;
};

struct MapDeltaPacket {
	uint8_t type;
	uint8_t count;
	struct TileDelta cells[127];
};

static bool terrain_known[MAX_MAP_SIZE][MAX_MAP_SIZE];
static uint8_t terrain_type[MAX_MAP_SIZE][MAX_MAP_SIZE];
static bool resource_known[MAX_MAP_SIZE][MAX_MAP_SIZE];
static bool base_known[MAX_MAP_SIZE][MAX_MAP_SIZE];

static int bfs_queue[MAX_CELLS];
static bool bfs_visited[MAX_CELLS];
static int bfs_first_dir[MAX_CELLS];

static bool home_set = false;
static int home_x = 0;
static int home_y = 0;

static int map_width = 0;
static int map_height = 0;

static int last_pos_x = -1;
static int last_pos_y = -1;
static int stuck_turns = 0;
static int wait_turns = 0;
static int scout_turns = 0;
static int harvest_cycles = 0;

static uint8_t msg_buffer[MSG_MAX_LEN];

static int randomDirOrder[4];

static int pending_target_x = -1;
static int pending_target_y = -1;

static const char *modeNameOf(int mode) {
	if (mode == MODE_TO_RESOURCE) {
		return "to_resource";
	}
	if (mode == MODE_TO_BASE) {
		return "to_base";
	}
	if (mode == MODE_SCOUT) {
		return "scout";
	}
	return "explore";
}

static int toIndex(int x, int y) {
	return y * map_width + x;
}

static bool inBounds(int x, int y) {
	return x >= 0 && x < map_width && y >= 0 && y < map_height;
}

static bool isWalkableKnown(int x, int y) {
	if (!inBounds(x, y)) {
		return false;
	}
	if (!terrain_known[y][x]) {
		return false;
	}
	return terrain_type[y][x] != TERRAIN_WATER
		&& terrain_type[y][x] != TERRAIN_OBSTACLE;
}

static bool isVisibleOtherUnit(
	int target_x, int target_y, int self_x, int self_y, int radius
) {
	int side = radius * 2 + 1;
	int sx = target_x - self_x + radius;
	int sy = target_y - self_y + radius;

	if (sx < 0 || sx >= side || sy < 0 || sy >= side) {
		return false;
	}

	int idx = sy * side + sx;
	if (!sensor_data[idx].visible) {
		return false;
	}
	if (!sensor_data[idx].has_unit) {
		return false;
	}

	return !(target_x == self_x && target_y == self_y);
}

static bool cellHasVisibleUnit(
	int target_x, int target_y, int self_x, int self_y, int radius
) {
	int side = radius * 2 + 1;
	int sx = target_x - self_x + radius;
	int sy = target_y - self_y + radius;

	if (sx < 0 || sx >= side || sy < 0 || sy >= side) {
		return false;
	}

	int idx = sy * side + sx;
	if (!sensor_data[idx].visible) {
		return false;
	}
	return sensor_data[idx].has_unit;
}

static bool isPathWalkable(int x, int y, int self_x, int self_y, int radius) {
	if (!isWalkableKnown(x, y)) {
		return false;
	}
	if (isVisibleOtherUnit(x, y, self_x, self_y, radius)) {
		return false;
	}
	return true;
}

static bool hasUnknownNeighbor(int x, int y) {
	for (int dir = 0; dir < 4; dir++) {
		int nx = x + DIR_X_OFFSET_OF(dir);
		int ny = y + DIR_Y_OFFSET_OF(dir);
		if (!inBounds(nx, ny)) {
			continue;
		}
		if (!terrain_known[ny][nx]) {
			return true;
		}
	}
	return false;
}

static bool hasKnownResource(void) {
	for (int y = 0; y < map_height; y++) {
		for (int x = 0; x < map_width; x++) {
			if (resource_known[y][x]) {
				return true;
			}
		}
	}
	return false;
}

static bool hasKnownBase(void) {
	for (int y = 0; y < map_height; y++) {
		for (int x = 0; x < map_width; x++) {
			if (base_known[y][x]) {
				return true;
			}
		}
	}
	return false;
}

static void fillRandomDirOrder(void) {
	for (int i = 0; i < 4; i++) {
		randomDirOrder[i] = i;
	}

	// Fisher-Yates shuffle for randomized tie-breaking in BFS expansion.
	for (int i = 3; i > 0; i--) {
		int j = (int)(rand() % (uint32_t)(i + 1));
		int tmp = randomDirOrder[i];
		randomDirOrder[i] = randomDirOrder[j];
		randomDirOrder[j] = tmp;
	}
}

static bool isTargetCell(int idx, int target_mode, int target_x, int target_y) {
	int x = idx % map_width;
	int y = idx / map_width;

	if (target_mode == MODE_TO_RESOURCE) {
		return resource_known[y][x];
	}
	if (target_mode == MODE_TO_BASE) {
		if (hasKnownBase()) {
			return base_known[y][x];
		}
		return x == target_x && y == target_y;
	}
	if (target_mode == MODE_EXPLORE || target_mode == MODE_SCOUT) {
		return hasUnknownNeighbor(x, y);
	}
	return false;
}

static int bfsDirToMode(
	int start_x, int start_y, int self_x, int self_y, int radius,
	int target_mode, int target_x, int target_y
) {
	int head = 0;
	int tail = 0;
	int start_idx = toIndex(start_x, start_y);
	int total_cells = map_width * map_height;

	fillRandomDirOrder();

	for (int i = 0; i < total_cells; i++) {
		bfs_visited[i] = false;
		bfs_first_dir[i] = -1;
	}

	bfs_visited[start_idx] = true;
	bfs_queue[tail++] = start_idx;

	while (head < tail) {
		int idx = bfs_queue[head++];

		if (isTargetCell(idx, target_mode, target_x, target_y)) {
			pending_target_x = idx % map_width;
			pending_target_y = idx / map_width;
			return bfs_first_dir[idx];
		}

		int x = idx % map_width;
		int y = idx / map_width;
		for (int i = 0; i < 4; i++) {
			int dir = randomDirOrder[i];
			int nx = x + DIR_X_OFFSET_OF(dir);
			int ny = y + DIR_Y_OFFSET_OF(dir);
			if (!isPathWalkable(nx, ny, self_x, self_y, radius)) {
				continue;
			}
			int nidx = toIndex(nx, ny);
			if (bfs_visited[nidx]) {
				continue;
			}

			bfs_visited[nidx] = true;
			bfs_first_dir[nidx] = (idx == start_idx) ? dir : bfs_first_dir[idx];
			bfs_queue[tail++] = nidx;
		}
	}

	pending_target_x = -1;
	pending_target_y = -1;
	return -1;
}

static int nearestResourceDir(
	int start_x, int start_y, int self_x, int self_y, int radius
) {
	return bfsDirToMode(
		start_x, start_y, self_x, self_y, radius, MODE_TO_RESOURCE, -1, -1
	);
}

static int frontierDir(
	int start_x, int start_y, int self_x, int self_y, int radius
) {
	return bfsDirToMode(
		start_x, start_y, self_x, self_y, radius, MODE_EXPLORE, -1, -1
	);
}

static int pointDir(
	int start_x, int start_y, int target_x, int target_y, int self_x,
	int self_y, int radius
) {
	if (!inBounds(target_x, target_y)) {
		return -1;
	}
	if (!isPathWalkable(target_x, target_y, self_x, self_y, radius)
	    && !(target_x == self_x && target_y == self_y)) {
		return -1;
	}

	int head = 0;
	int tail = 0;
	int start_idx = toIndex(start_x, start_y);
	int target_idx = toIndex(target_x, target_y);
	int total_cells = map_width * map_height;

	fillRandomDirOrder();

	for (int i = 0; i < total_cells; i++) {
		bfs_visited[i] = false;
		bfs_first_dir[i] = -1;
	}

	bfs_visited[start_idx] = true;
	bfs_queue[tail++] = start_idx;

	while (head < tail) {
		int idx = bfs_queue[head++];
		if (idx == target_idx) {
			return bfs_first_dir[idx];
		}

		int x = idx % map_width;
		int y = idx / map_width;
		for (int i = 0; i < 4; i++) {
			int dir = randomDirOrder[i];
			int nx = x + DIR_X_OFFSET_OF(dir);
			int ny = y + DIR_Y_OFFSET_OF(dir);
			if (!isPathWalkable(nx, ny, self_x, self_y, radius)) {
				continue;
			}
			int nidx = toIndex(nx, ny);
			if (bfs_visited[nidx]) {
				continue;
			}

			bfs_visited[nidx] = true;
			bfs_first_dir[nidx] = (idx == start_idx) ? dir : bfs_first_dir[idx];
			bfs_queue[tail++] = nidx;
		}
	}

	return -1;
}

static bool isInResource(struct PosInfo p) {
	if (!inBounds((int)p.x, (int)p.y)) {
		return false;
	}
	return resource_known[p.y][p.x];
}

static bool isInBase(struct PosInfo p) {
	if (!inBounds((int)p.x, (int)p.y)) {
		return false;
	}
	return base_known[p.y][p.x];
}

static int capacityOf(struct DeviceInfo info) {
	return HAS_UPGRADE(info.upgrades, UPGRADE_CAPACITY) ? 1000 : 200;
}

static bool centerIsBase(const struct SensorData *data, int radius) {
	int side = radius * 2 + 1;
	int center = radius * side + radius;
	if (!data[center].visible) {
		return false;
	}
	return data[center].is_base;
}

static bool localStepBlocked(
	const struct SensorData *data, int radius, int dir
) {
	int dx = DIR_X_OFFSET_OF(dir);
	int dy = DIR_Y_OFFSET_OF(dir);
	int side = radius * 2 + 1;
	int cx = radius;
	int cy = radius;
	int nx = cx + dx;
	int ny = cy + dy;
	int nidx = ny * side + nx;

	if (nidx < 0 || nidx >= side * side) {
		return true;
	}
	if (!data[nidx].visible) {
		return false;
	}
	if (data[nidx].terrain == TERRAIN_WATER
	    || data[nidx].terrain == TERRAIN_OBSTACLE) {
		return true;
	}
	if (data[nidx].has_unit) {
		return true;
	}
	return false;
}

static bool shouldYieldToBlocker(
	int self_x, int self_y, int dir, int radius, int current_turn
) {
	int nx = self_x + DIR_X_OFFSET_OF(dir);
	int ny = self_y + DIR_Y_OFFSET_OF(dir);
	if (!cellHasVisibleUnit(nx, ny, self_x, self_y, radius)) {
		return false;
	}

	// Only one side yields: lexicographically larger coordinate backs off.
	if (self_x > nx || (self_x == nx && self_y > ny)) {
		return true;
	}

	// Deterministic tiebreaker for very rare same-coordinate ambiguities.
	return ((self_x + self_y + current_turn) & 1) == 0;
}

static int localFallbackDir(const struct SensorData *data, int radius) {
	fillRandomDirOrder();
	for (int i = 0; i < 4; i++) {
		int dir = randomDirOrder[i];
		if (!localStepBlocked(data, radius, dir)) {
			return dir;
		}
	}
	return -1;
}

static uint8_t encodedTerrain(uint8_t terrain) {
	if (terrain == TERRAIN_WATER) {
		return CELL_WATER;
	}
	if (terrain == TERRAIN_OBSTACLE) {
		return CELL_OBSTACLE;
	}
	return CELL_EMPTY;
}

static uint8_t packCellBits(const struct SensorData *cell) {
	uint8_t bits = encodedTerrain(cell->terrain);
	if (cell->is_resource) {
		bits |= (1u << 2);
	}
	if (cell->is_base) {
		bits |= (1u << 3);
	}
	return bits;
}

static void applyPackedCell(int x, int y, uint8_t bits) {
	if (!inBounds(x, y)) {
		return;
	}

	terrain_known[y][x] = true;
	uint8_t t = (uint8_t)(bits & 0x3);
	if (t == CELL_WATER) {
		terrain_type[y][x] = TERRAIN_WATER;
	} else if (t == CELL_OBSTACLE) {
		terrain_type[y][x] = TERRAIN_OBSTACLE;
	} else {
		terrain_type[y][x] = TERRAIN_EMPTY;
	}

	if (bits & (1u << 2)) {
		resource_known[y][x] = true;
	}
	if (bits & (1u << 3)) {
		base_known[y][x] = true;
	}
}

static void processMessages(void) {
	while (true) {
		int n = recv_msg(msg_buffer, MSG_MAX_LEN);
		if (n <= 0) {
			break;
		}
		if (n < 2) {
			continue;
		}

		if (msg_buffer[0] == PACKET_MAP_DELTA) {
			int count = msg_buffer[1];
			int expected = 2 + count * 3;
			if (expected > n) {
				continue;
			}

			int off = 2;
			for (int i = 0; i < count; i++) {
				int x = msg_buffer[off++];
				int y = msg_buffer[off++];
				uint8_t bits = msg_buffer[off++];
				applyPackedCell(x, y, bits);
			}
		}
	}
}

static void broadcastVisibleMap(
	const struct PosInfo p, const struct DeviceInfo info
) {
	int radius = HAS_UPGRADE(info.upgrades, UPGRADE_VISION) ? 4 : 2;
	int side = radius * 2 + 1;
	struct MapDeltaPacket pkt;
	pkt.type = PACKET_MAP_DELTA;
	pkt.count = 0;

	for (int sy = 0; sy < side; sy++) {
		for (int sx = 0; sx < side; sx++) {
			int idx = sy * side + sx;
			if (!sensor_data[idx].visible) {
				continue;
			}

			int gx = (int)p.x + (sx - radius);
			int gy = (int)p.y + (sy - radius);
			if (!inBounds(gx, gy)) {
				continue;
			}

			if (pkt.count >= 127) {
				break;
			}

			pkt.cells[pkt.count].x = (uint8_t)gx;
			pkt.cells[pkt.count].y = (uint8_t)gy;
			pkt.cells[pkt.count].bits = packCellBits(&sensor_data[idx]);
			pkt.count++;
		}
	}

	if (pkt.count > 0) {
		int len = 2 + pkt.count * 3;
		send_msg((const uint8_t *)&pkt, len);
	}
}

static void updateMap(const struct PosInfo p, const struct DeviceInfo info) {
	int radius = HAS_UPGRADE(info.upgrades, UPGRADE_VISION) ? 4 : 2;
	int side = radius * 2 + 1;

	read_sensor(sensor_data);

	for (int sy = 0; sy < side; sy++) {
		for (int sx = 0; sx < side; sx++) {
			int idx = sy * side + sx;
			if (!sensor_data[idx].visible) {
				continue;
			}

			int gx = (int)p.x + (sx - radius);
			int gy = (int)p.y + (sy - radius);
			if (!inBounds(gx, gy)) {
				continue;
			}

			terrain_known[gy][gx] = true;
			terrain_type[gy][gx] = sensor_data[idx].terrain;
			if (sensor_data[idx].is_resource) {
				resource_known[gy][gx] = true;
			}
			if (sensor_data[idx].is_base) {
				base_known[gy][gx] = true;
			}
		}
	}
}

int main(void) {
	if (meta(&game_info) != 0) {
		while (true) {}
	}

	map_width = game_info.map_width;
	map_height = game_info.map_height;

	int last_turn = 0;

	while (true) {
		int current_turn = turn();
		if (current_turn == last_turn) {
			continue;
		}
		last_turn = current_turn;

		struct PosInfo p = pos();
		struct DeviceInfo info = dev_info();
		processMessages();

		if (!home_set) {
			home_x = p.x;
			home_y = p.y;
			home_set = true;
		}

		updateMap(p, info);

		int radius = HAS_UPGRADE(info.upgrades, UPGRADE_VISION) ? 4 : 2;
		if (last_pos_x == (int)p.x && last_pos_y == (int)p.y) {
			stuck_turns++;
		} else {
			stuck_turns = 0;
		}
		last_pos_x = p.x;
		last_pos_y = p.y;

		bool in_base = centerIsBase(sensor_data, radius) || isInBase(p);
		bool in_resource = isInResource(p);
		int carry_capacity = capacityOf(info);
		int return_threshold = (carry_capacity * 9) / 10;

		if (in_base && info.energy > 0) {
			int dep_ret = deposit(info.energy);
			if (dep_ret != EC_OK) {
				logf(
					"[hv] t=%d deposit(%d)=%d\n", current_turn, info.energy,
					dep_ret
				);
			} else {
				harvest_cycles++;
				if ((harvest_cycles % 3) == 0) {
					scout_turns = 100;
				}
			}
			info = dev_info();
		}

		int mode = MODE_EXPLORE;
		if (scout_turns > 0 && info.energy < return_threshold) {
			mode = MODE_SCOUT;
		} else if (hasKnownResource()) {
			if (info.energy >= return_threshold) {
				mode = MODE_TO_BASE;
			} else if (in_resource) {
				mode = MODE_EXPLORE;
			} else {
				mode = MODE_TO_RESOURCE;
			}
		}

		int dir = -1;
		if (mode == MODE_TO_BASE) {
			dir = bfsDirToMode(
				p.x, p.y, p.x, p.y, radius, MODE_TO_BASE, home_x, home_y
			);
			if (dir < 0) {
				dir = pointDir(p.x, p.y, home_x, home_y, p.x, p.y, radius);
			}
		} else if (mode == MODE_TO_RESOURCE) {
			dir = nearestResourceDir(p.x, p.y, p.x, p.y, radius);
			if (dir < 0) {
				dir = frontierDir(p.x, p.y, p.x, p.y, radius);
			}
		} else if (mode == MODE_SCOUT) {
			dir = frontierDir(p.x, p.y, p.x, p.y, radius);
			if (dir < 0 && hasKnownResource()) {
				dir = nearestResourceDir(p.x, p.y, p.x, p.y, radius);
			}
		} else {
			// Stay on resource tile to harvest until near capacity.
			if (!in_resource) {
				dir = frontierDir(p.x, p.y, p.x, p.y, radius);
			}
		}

		if (dir >= 0
		    && shouldYieldToBlocker(p.x, p.y, dir, radius, current_turn)
		    && stuck_turns >= 1) {
			wait_turns = 2;
		}

		if (dir < 0 || localStepBlocked(sensor_data, radius, dir)) {
			if (stuck_turns >= 2 && wait_turns == 0) {
				dir = localFallbackDir(sensor_data, radius);
			}
		}

		if (mode == MODE_SCOUT && scout_turns > 0) {
			scout_turns--;
		}

		broadcastVisibleMap(p, info);

		if (current_turn <= 20 || (current_turn % 25) == 0
		    || stuck_turns >= 3) {
			logf(
				"[hv] t=%d pos=(%d,%d) e=%d mode=%s dir=%d stuck=%d wait=%d "
				"scout=%d target=(%d,%d)\n",
				current_turn, p.x, p.y, info.energy, modeNameOf(mode), dir,
				stuck_turns, wait_turns, scout_turns, pending_target_x,
				pending_target_y
			);
		}

		if (wait_turns > 0) {
			wait_turns--;
		} else if (dir >= 0 && !localStepBlocked(sensor_data, radius, dir)) {
			move(dir);
		} else if (stuck_turns >= 3) {
			logf("[hv] t=%d no-move (blocked or no path)\n", current_turn);
		}
	}
}
