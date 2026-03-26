#include "corelib.h"

static uint8_t message_buf[32];

static void assertMessageEquals(const uint8_t *buf, int n, const char *text) {
	int expected_len = strlen(text);
	assert(n == expected_len);
	assert(memcmp(buf, text, expected_len) == 0);
}

int main(void) {
	struct DeviceInfo info = dev_info();
	int my_id = info.id;
	assert(my_id >= 1 && my_id <= 3);

	int last_turn = 0;
	int received_ok = 0;

	while (true) {
		int t = turn();
		if (t == last_turn) {
			continue;
		}
		last_turn = t;

		if (my_id == 1 && t == 1) {
			assert(send_msg((const uint8_t *)"PING", 4) == EC_OK);
			assert(send_msg((const uint8_t *)"PING", 4) == EC_ON_COOLDOWN);
		}

		if (!received_ok && t >= 2) {
			int n = recv_msg(message_buf, sizeof(message_buf));
			if (n > 0) {
				assertMessageEquals(message_buf, n, "PING");
				received_ok = 1;
			}
		}

		if (!received_ok && t >= 6) {
			assert(false);
		}

		if (received_ok && t >= 6) {
			log_str("[PASS] msg ecall behavior is correct.");
			while (true) {}
		}
	}
}
