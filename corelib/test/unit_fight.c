// unit_fight.c -- unit: tests read_sensor(), move(), fire() across multiple
// ticks. Only unit id=1 performs verbose logging and fire tests. Units 2 and 3
// move silently and serve as additional targets for sensor data.
#include "corelib.h"

static char buf[128];

static char *append_str(char *p, const char *s) {
	while (*s) {
		*p++ = *s++;
	}
	return p;
}

static char *append_int(char *p, int v) {
	if (v < 0) {
		*p++ = '-';
		v = -v;
	}
	char tmp[12];
	int i = 0;
	if (v == 0) {
		*p++ = '0';
		return p;
	}
	while (v > 0) {
		tmp[i++] = '0' + (v % 10);
		v /= 10;
	}
	while (i > 0) {
		*p++ = tmp[--i];
	}
	return p;
}

// 5x5 sensor area (no vision upgrade).
static struct SensorData sensor[25];

// Log the sensor area as a 5x5 ASCII map.
static void log_sensor_map(int my_id, int t) {
	char *p = buf;
	p = append_str(p, "[fight] unit=");
	p = append_int(p, my_id);
	p = append_str(p, " tick=");
	p = append_int(p, t);
	p = append_str(p, " sensor map (center=self):\n");
	log(buf, p - buf);

	for (int row = 0; row < 5; row++) {
		p = buf;
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

int main() {
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
			char *ptr = buf;
			ptr = append_str(ptr, "[fight] unit=");
			ptr = append_int(ptr, my_id);
			ptr = append_str(ptr, " tick=");
			ptr = append_int(ptr, t);
			ptr = append_str(ptr, " pos=(");
			ptr = append_int(ptr, p.x);
			ptr = append_str(ptr, ",");
			ptr = append_int(ptr, p.y);
			ptr = append_str(ptr, ") sensor_tiles=");
			ptr = append_int(ptr, n);
			*ptr++ = '\n';
			log(buf, ptr - buf);
		}

		// --- Test: move ---
		// Cycle through four directions to exercise all movement directions.
		int dir = (t - born_turn) % 4;
		int mv_ret = move(move_cycle[dir]);

		if (my_id == 1) {
			char *ptr = buf;
			ptr = append_str(ptr, "[fight] unit=");
			ptr = append_int(ptr, my_id);
			ptr = append_str(ptr, " tick=");
			ptr = append_int(ptr, t);
			ptr = append_str(ptr, " move(dir=");
			ptr = append_int(ptr, move_cycle[dir]);
			ptr = append_str(ptr, ")=");
			ptr = append_int(ptr, mv_ret);
			*ptr++ = '\n';
			log(buf, ptr - buf);
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
				char *ptr = buf;
				ptr = append_str(ptr, "[fight] unit=");
				ptr = append_int(ptr, my_id);
				ptr = append_str(ptr, " tick=");
				ptr = append_int(ptr, t);
				ptr = append_str(ptr, " fire(dir=");
				ptr = append_int(ptr, fdir);
				ptr = append_str(ptr, ",power=1)=");
				ptr = append_int(ptr, fire_ret);
				*ptr++ = '\n';
				log(buf, ptr - buf);
			}
		}
	}
}
