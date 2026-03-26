#include "corelib.h"

int main(void) {
	struct DeviceInfo info = dev_info();
	if (info.id != 1) {
		while (true) {}
	}

	int last_turn = 0;
	int phase = 0;

	while (true) {
		int t = turn();
		if (t == last_turn) {
			continue;
		}
		last_turn = t;

		info = dev_info();
		int energy_before = info.energy;

		if (phase == 0) {
			if (t >= 9) {
				phase = 1;
			}
			continue;
		}

		if (phase == 1) {
			assert(energy_before >= 5);
			assert(deposit(5) == EC_OK);
			assert(dev_info().energy == energy_before - 5);
			phase = 2;
			continue;
		}

		if (phase == 2) {
			assert(withdraw(10) == EC_INSUFFICIENT_ENERGY);
			assert(withdraw(3) == EC_OK);
			assert(dev_info().energy == energy_before + 3);
			phase = 3;
			continue;
		}

		if (phase == 3) {
			assert(deposit(0) == EC_OUT_OF_RANGE);
			assert(deposit(3) == EC_OK);
			assert(dev_info().energy == energy_before - 3);
			phase = 4;
			continue;
		}

		if (phase == 4) {
			assert(deposit(1) == EC_OK);
			assert(deposit(1) == EC_ON_COOLDOWN);
			assert(withdraw(1) == EC_ON_COOLDOWN);
			assert(dev_info().energy == energy_before - 1);
			phase = 5;
			continue;
		}

		if (phase == 5) {
			log_str("[PASS] deposit/withdraw ecall behavior is correct.");
			while (true) {}
		}
	}
}
