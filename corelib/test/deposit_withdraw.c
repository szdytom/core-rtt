// deposit_withdraw.c -- unit id=1: tests deposit() and withdraw() semantics.
// Units 2 and 3 stay idle.
//
// Test sequence (unit stays in base area throughout; no move() called):
//   ticks  1-9 : accumulate energy, log dev_info each tick
//   tick    10 : deposit(5)              → EC_OK     (base: 0→5, unit: 10→5)
//   tick    11 : withdraw(10)            → EC_INSUFFICIENT_ENERGY (base only 5)
//                withdraw(3)             → EC_OK     (base: 5→2, unit: 6→9)
//   tick    12 : deposit(0)              → EC_OUT_OF_RANGE
//                deposit(3)             → EC_OK     (base: 2→5, unit: 10→7)
//   tick    13 : deposit(1)             → EC_OK   (flag set)
//                deposit(1) again        → EC_ON_COOLDOWN
//                withdraw(1) in same tick→ EC_ON_COOLDOWN

#include "corelib.h"

static char line[96];

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

// Log a single call result: "[dw] tick=T op(arg)=ret\n"
static void log_call(int t, const char *op, int arg, int ret) {
	char *p = line;
	p = append_str(p, "[dw] tick=");
	p = append_int(p, t);
	p = append_str(p, " ");
	p = append_str(p, op);
	p = append_str(p, "(");
	p = append_int(p, arg);
	p = append_str(p, ")=");
	p = append_int(p, ret);
	*p++ = '\n';
	log(line, p - line);
}

static void log_energy(int t, int energy) {
	char *p = line;
	p = append_str(p, "[dw] tick=");
	p = append_int(p, t);
	p = append_str(p, " unit_energy=");
	p = append_int(p, energy);
	*p++ = '\n';
	log(line, p - line);
}

int main() {
	struct DeviceInfo info = dev_info();
	if (info.id != 1) {
		// Other units stay idle.
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

		// Refresh energy on every tick.
		info = dev_info();
		int energy = info.energy;

		switch (phase) {
		case 0:
			// Accumulate energy for 9 ticks.
			log_energy(t, energy);
			if (t >= 9) {
				phase = 1;
			}
			break;

		case 1: {
			// deposit(5): unit has 10 energy (9 ticks + 1 on tick 10).
			log_energy(t, energy);
			int ret = deposit(5);
			log_call(t, "deposit", 5, ret);
			phase = 2;
			break;
		}

		case 2: {
			// withdraw(10): base only has 5 → EC_INSUFFICIENT_ENERGY.
			log_energy(t, energy);
			int ret1 = withdraw(10);
			log_call(t, "withdraw", 10, ret1);
			// Flag not set on failure; can retry in same tick.
			int ret2 = withdraw(3);
			log_call(t, "withdraw", 3, ret2);
			phase = 3;
			break;
		}

		case 3: {
			// deposit(0) → EC_OUT_OF_RANGE; then deposit(3) → EC_OK.
			log_energy(t, energy);
			int ret1 = deposit(0);
			log_call(t, "deposit", 0, ret1);
			// deposit(0) failed (OUT_OF_RANGE), flag not set; can still
			// deposit.
			int ret2 = deposit(3);
			log_call(t, "deposit", 3, ret2);
			phase = 4;
			break;
		}

		case 4: {
			// deposit(1) succeeds; subsequent deposit and withdraw both return
			// EC_ON_COOLDOWN within the same tick.
			log_energy(t, energy);
			int ret1 = deposit(1);
			log_call(t, "deposit", 1, ret1); // EC_OK
			int ret2 = deposit(1);
			log_call(t, "deposit", 1, ret2); // EC_ON_COOLDOWN
			int ret3 = withdraw(1);
			log_call(t, "withdraw", 1, ret3); // EC_ON_COOLDOWN
			phase = 5;
			break;
		}

		default:
			// All phases done; log final energy once, then idle quietly.
			if (phase == 5) {
				log_energy(t, energy);
				log_str("All deposit/withdraw tests done.\n");
				phase = 6;
			}
			break;
		}
	}
}
