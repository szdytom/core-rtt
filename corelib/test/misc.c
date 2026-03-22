// misc.c -- player2 base: tests turn() and rand() each tick.
#include "corelib.h"

int main() {
	int last_turn = 0;

	while (true) {
		int t = turn();
		if (t == last_turn) {
			continue;
		}
		last_turn = t;

		// Log turn number
		logf("[misc] tick=%d\n", t);

		// Generate and log 3 random numbers
		for (int i = 0; i < 3; i++) {
			uint32_t r = rand();
			logf("[misc] rand[%d]=0x%x\n", i, r);
		}
	}
}
