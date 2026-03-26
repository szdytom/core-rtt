#include "corelib.h"

static char payload[512];

int main(void) {
	for (int i = 0; i < (int)sizeof(payload); i++) {
		payload[i] = 'L';
	}

	int last_turn = 0;
	int phase = 0;

	while (true) {
		int t = turn();
		if (t == last_turn) {
			continue;
		}
		last_turn = t;

		if (phase == 0) {
			assert(t == 1);
			// 4 * 512 = 2048, should exactly drain the initial quota.
			for (int i = 0; i < 4; i++) {
				assert(log(payload, 512) == EC_OK);
			}
			// Quota regenerated from 1st tick
			assert(log(payload, 32) == EC_OK);
			// Quota should be drained by now
			assert(log(payload, 1) == EC_ON_COOLDOWN);
			assert(log(payload, 513) == EC_OUT_OF_RANGE);
			phase = 1;
			continue;
		}

		if (phase == 1) {
			// Next turn adds 32 quota.
			assert(log(payload, 33) == EC_ON_COOLDOWN);
			assert(log(payload, 32) == EC_OK);
			phase = 2;
			continue;
		}

		if (phase == 2) {
			// Refill is per-turn and repeatable.
			assert(log(payload, 33) == EC_ON_COOLDOWN);
			assert(log(payload, 32) == EC_OK);
			phase = 3;
			continue;
		}

		if (phase == 3) {
			log_str("[PASS] log quota works as expected.");
			continue;
		}
	}
	return 0;
}
