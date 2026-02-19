// msg.c -- player1 unit: tests dev_info(), send_msg() and recv_msg().
#include "corelib.h"

static char *append_str(char *p, const char *s) {
	while (*s) {
		*p++ = *s++;
	}
	return p;
}

static char *append_uint(char *p, unsigned int v) {
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

static char line[128];
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
		{
			char *p = line;
			p = append_str(p, "[msg] unit=");
			p = append_uint(p, my_id);
			p = append_str(p, " tick=");
			p = append_uint(p, t);
			p = append_str(p, " hp=");
			p = append_uint(p, info.health);
			p = append_str(p, " energy=");
			p = append_uint(p, info.energy);
			*p++ = '\n';
			log(line, p - line);
		}

		// Unit 1 sends a broadcast message each tick.
		if (my_id == 1) {
			char *p = (char *)msg_buf;
			p = append_str(p, "hello from unit 1, tick=");
			p = append_uint(p, t);
			send_msg(msg_buf, p - (char *)msg_buf);
		}

		// All units drain their incoming message queue and log what they get.
		while (true) {
			int n = recv_msg(msg_buf, sizeof(msg_buf) - 1);
			if (n <= 0) {
				break;
			}
			msg_buf[n] = '\0';
			char *p = line;
			p = append_str(p, "[msg] unit=");
			p = append_uint(p, my_id);
			p = append_str(p, " recv: ");
			p = append_str(p, (const char *)msg_buf);
			*p++ = '\n';
			log(line, p - line);
		}

		// Re-read dev_info each tick to get updated energy.
		info = dev_info();
	}
}
