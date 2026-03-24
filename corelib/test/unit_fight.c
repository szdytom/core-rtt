// unit_fight.c -- unit: tests read_sensor(), move(), fire() across multiple
// ticks. Only unit id=1 performs verbose logging and fire tests. Units 2 and 3
// move silently and serve as additional targets for sensor data.
#include "corelib.h"

static char buf[128];

// 5x5 sensor area (no vision upgrade).
static struct SensorData sensor[25];

// Log the sensor area as a 5x5 ASCII map.
static void log_sensor_map(int my_id, int t) {
	logf("[fight] unit=%d tick=%d sensor map (center=self):\n", my_id, t);

	for (int row = 0; row < 5; row++) {
		char *p = buf;
		for (int col = 0; col < 5; col++) {
			struct SensorData *d = &sensor[row * 5 + col];
			char ch;
			if (!d->visible) {
				ch = '?';
			} else if (d->terrain == TERRAIN_OBSTACLE) {
				ch = '#';
			} else if (d->terrain == TERRAIN_WATER) {
				ch = '~';
			} else if (row == 2 && col == 2) {
				ch = '@'; // self
			} else if (d->has_bullet) {
				ch = '*';
			} else if (d->has_unit) {
				ch = d->alliance_unit ? 'A' : 'E';
			} else if (d->is_base) {
				ch = 'B';
			} else if (d->is_resource) {
				ch = 'R';
			} else {
				ch = '.';
			}
			*p++ = ch;
			*p++ = (col == 4) ? '\n' : ' ';
		}
		log(buf, p - buf);
	}
}

// Directions to cycle through each tick: right, down, left, up.
static const int move_cycle[4] = {DIR_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP};

int main(void) {
	struct DeviceInfo info = dev_info();
	int my_id = info.id;

	int last_turn = 0;
	// Record the first tick this unit is alive to compute fire schedule.
	int born_turn = 0;

	while (true) {
		int t = turn();
		if (t == last_turn) {
			continue;
		}
		last_turn = t;

		if (born_turn == 0) {
			born_turn = t;
		}

		// --- Test: read_sensor ---
		int n = read_sensor(sensor);

		if (my_id == 1) {
			log_sensor_map(my_id, t);

			struct PosInfo p = pos();
			logf(
				"[fight] unit=%d tick=%d pos=(%d,%d) sensor_tiles=%d\n", my_id,
				t, p.x, p.y, n
			);
		}

		// --- Test: move ---
		// Cycle through four directions to exercise all movement directions.
		int dir = (t - born_turn) % 4;
		int mv_ret = move(move_cycle[dir]);

		if (my_id == 1) {
			logf(
				"[fight] unit=%d tick=%d move(dir=%d)=%d\n", my_id, t,
				move_cycle[dir], mv_ret
			);
		}

		// --- Test: fire ---
		// All units fire every 3 ticks (cooldown = 2 turns after a successful
		// fire). Use power=1 so firing is possible from the very first tick.
		// Different unit ids fire in different directions to maximise the
		// chance of bullets crossing the 5x5 sensor area of unit 1.
		static const int fire_dirs[3] = {DIR_RIGHT, DIR_DOWN, DIR_LEFT};
		int fdir = fire_dirs[(my_id - 1) % 3];
		if ((t - born_turn) % 3 == 0) {
			int fire_ret = fire(fdir, 1);
			if (my_id == 1) {
				logf(
					"[fight] unit=%d tick=%d fire(dir=%d,power=1)=%d\n", my_id,
					t, fdir, fire_ret
				);
			}
		}
	}
}
