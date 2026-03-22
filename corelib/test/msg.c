// msg.c -- player1 unit: tests dev_info(), send_msg() and recv_msg().
#include "corelib.h"
static uint8_t msg_buf[64];

int main() {
	// Snapshot device info once at startup: id is stable throughout unit life.
	struct DeviceInfo info = dev_info();
	int my_id = info.id;

	int last_turn = 0;

	while (true) {
		int t = turn();
		if (t == last_turn) {
			continue;
		}
		last_turn = t;

		// Log this unit's status (id, health, energy) every tick.
		logf("[msg] unit=%d tick=%d hp=%d energy=%d\n", my_id, t,
			info.health, info.energy);

		// Unit 1 sends a broadcast message each tick.
		if (my_id == 1) {
			int msg_len = fmt_str((char *)msg_buf, sizeof(msg_buf),
				"hello from unit 1, tick=%d", t);
			send_msg(msg_buf, msg_len);
		}

		// All units drain their incoming message queue and log what they get.
		while (true) {
			int n = recv_msg(msg_buf, sizeof(msg_buf) - 1);
			if (n <= 0) {
				break;
			}
			msg_buf[n] = '\0';
			logf("[msg] unit=%d recv: %s\n", my_id, (const char *)msg_buf);
		}

		// Re-read dev_info each tick to get updated energy.
		info = dev_info();
	}
}
