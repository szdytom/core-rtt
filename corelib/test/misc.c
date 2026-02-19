// misc.c -- player2 base: tests turn() and rand() each tick.
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

static char *append_hex32(char *p, uint32_t v) {
	static const char hex[] = "0123456789ABCDEF";
	*p++ = '0';
	*p++ = 'x';
	for (int i = 7; i >= 0; i--) {
		*p++ = hex[(v >> (i * 4)) & 0xF];
	}
	return p;
}

static char line[128];

int main() {
	int last_turn = 0;

	while (true) {
		int t = turn();
		if (t == last_turn) {
			continue;
		}
		last_turn = t;

		// Log turn number
		{
			char *p = line;
			p = append_str(p, "[misc] tick=");
			p = append_uint(p, t);
			*p++ = '\n';
			log(line, p - line);
		}

		// Generate and log 3 random numbers
		for (int i = 0; i < 3; i++) {
			uint32_t r = rand();
			char *p = line;
			p = append_str(p, "[misc] rand[");
			p = append_uint(p, i);
			p = append_str(p, "]=");
			p = append_hex32(p, r);
			*p++ = '\n';
			log(line, p - line);
		}
	}
}
