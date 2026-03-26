#include "corelib.h"

int main(void) {
	char buf[64];
	char small[8];
	char one[1];
	int len = 0;

	len = fmt_str(buf, sizeof(buf), "hello");
	assert(len == 5);
	assert(strcmp(buf, "hello") == 0);

	len = fmt_str(
		buf, sizeof(buf), "d=%d u=%u x=%x c=%c s=%s %%", -42, 42U, 0x1afU, 'Z',
		"ok"
	);
	assert(len == 27);
	assert(strcmp(buf, "d=-42 u=42 x=1af c=Z s=ok %") == 0);

	len = fmt_str(buf, sizeof(buf), "p=%p", NULL);
	assert(len == 12);
	assert(strcmp(buf, "p=0x00000000") == 0);

	len = fmt_str(buf, sizeof(buf), "unsupported:%q:end", 123);
	assert(len == 18);
	assert(strcmp(buf, "unsupported:%q:end") == 0);

	len = fmt_str(buf, sizeof(buf), "tail%");
	assert(len == 5);
	assert(strcmp(buf, "tail%") == 0);

	len = fmt_str(small, sizeof(small), "abcdefghi");
	assert(len == 7);
	assert(strcmp(small, "abcdefg") == 0);

	len = fmt_str(small, sizeof(small), "x=%x", 0x123456U);
	assert(len == 7);
	assert(strcmp(small, "x=12345") == 0);

	len = fmt_str(one, sizeof(one), "abc");
	assert(len == 0);
	assert(strcmp(one, "") == 0);

	assert(logf("[LOGF] d=%d x=%x s=%s %%", -7, 0x2aU, "ok") == EC_OK);
	log_str("[PASS] fmt_str behavior matches expected output\n");

	while (true) {}
	return 0;
}
