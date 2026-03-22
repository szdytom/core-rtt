// unit_random_walk.c -- unit runtime test: random walk with terrain-aware
// direction selection.
#include "corelib.h"

static struct SensorData sensor[25];

static bool isWalkable(const struct SensorData *d) {
	if (!d->visible) {
		return false;
	}
	if (d->terrain == TERRAIN_WATER || d->terrain == TERRAIN_OBSTACLE) {
		return false;
	}
	if (d->has_unit) {
		return false;
	}
	return true;
}

// Sensor indexing in 5x5 view: center is [2,2].
static int pickDirection(void) {
	static const int dirs[4] = {DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT};
	static const int sensor_index_of_dir[4] = {
		7,  // up    -> row=1,col=2
		13, // right -> row=2,col=3
		17, // down  -> row=3,col=2
		11, // left  -> row=2,col=1
	};

	int start = (int)(rand() & 3U);
	for (int i = 0; i < 4; i++) {
		int k = (start + i) & 3;
		if (isWalkable(&sensor[sensor_index_of_dir[k]])) {
			return dirs[k];
		}
	}
	return -1;
}

static void logStep(int pos_x, int pos_y, int dir, int ret) {
	const char *dir_str = "UDLR";
	if (ret != 0) {
		logf("[rw] pos=(%d,%d) dir=%c FAILED\n", pos_x, pos_y, dir_str[dir]);
		return;
	}
	logf("[rw] pos=(%d,%d) dir=%c\n", pos_x, pos_y, dir_str[dir]);
}

int main() {
	struct DeviceInfo info = dev_info();
	int unit_id = info.id;
	int last_turn = 0;

	while (true) {
		int t = turn();
		if (t == last_turn) {
			continue;
		}
		last_turn = t;

		read_sensor(sensor);
		int dir = pickDirection();
		if (dir >= 0) {
			int ret = move(dir);
			if (unit_id == 1) {
				struct PosInfo p = pos();
				logStep(p.x, p.y, dir, ret);
			}
		}
	}
}
