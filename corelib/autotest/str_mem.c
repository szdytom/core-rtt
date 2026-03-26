#include "corelib.h"

int main(void) {
	char a[16];
	char b[16];
	char c[16];

	// strlen
	assert(strlen("") == 0);
	assert(strlen("abc") == 3);

	// strcpy + strcmp
	assert(strcpy(a, "hello") == a);
	assert(strcmp(a, "hello") == 0);

	// strncmp
	assert(strncmp("hello", "heLLo", 2) == 0);
	assert(strncmp("hello", "heLLo", 3) != 0);

	// memset + memcmp
	memset(b, 'X', 5);
	assert(memcmp(b, "XXXXX", 5) == 0);
	b[5] = '\0';

	// memcpy + memcmp
	memcpy(c, b, 6);
	assert(memcmp(c, b, 6) == 0);

	// mixed semantics
	assert(strcmp(c, "XXXXX") == 0);

	log_str("[PASS] corelib string/memory functions work\n");

	while (true) {}
	return 0;
}
