// Manufacturing test: base. Manufacture a unit when energy is sufficient.

#include "corelib.h"
#define MANUFACTURE_COST 500

int main() {
	struct DeviceInfo info = dev_info();
	if (info.id != 0) {
		logf("Not running as base (id=%d)\n", info.id);
		return 1; // not running as base
	}

	int next_id = 4;
	int prev_tick = turn();
	int prev_energy = info.energy;
	while (true) {
		while (turn() == prev_tick)
			; // wait for next tick
		prev_tick = turn();

		if (prev_energy != info.energy) {
			logf(
				"%s: %d energy, current: %d\n",
				prev_energy > info.energy ? "Withdrew" : "Deposited",
				abs(info.energy - prev_energy), info.energy
			);
			prev_energy = info.energy;
		}

		info = dev_info();
		if (info.energy >= MANUFACTURE_COST) {
			int ret = manufact(next_id);
			logf("Manufacture unit %d: ret=%d\n", next_id, ret);
			if (ret == EC_OK) {
				prev_energy -= MANUFACTURE_COST;
			}

			next_id++;
			if (next_id >= 16) {
				next_id = 1;
			}
		}
	}
	return 0;
}