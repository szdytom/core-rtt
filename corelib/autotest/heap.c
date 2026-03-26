#include "corelib.h"

int main(void) {
	int *malloc_buf = (int *)malloc(8 * sizeof(int));
	assert(malloc_buf != NULL);
	for (int i = 0; i < 8; i++) {
		malloc_buf[i] = i * 3;
	}
	for (int i = 0; i < 8; i++) {
		assert(malloc_buf[i] == i * 3);
	}
	free(malloc_buf);

	int *calloc_buf = (int *)calloc(8, sizeof(int));
	assert(calloc_buf != NULL);
	for (int i = 0; i < 8; i++) {
		assert(calloc_buf[i] == 0);
	}
	free(calloc_buf);

	int *realloc_buf = (int *)malloc(4 * sizeof(int));
	assert(realloc_buf != NULL);
	for (int i = 0; i < 4; i++) {
		realloc_buf[i] = 100 + i;
	}

	realloc_buf = (int *)realloc(realloc_buf, 12 * sizeof(int));
	assert(realloc_buf != NULL);
	for (int i = 0; i < 4; i++) {
		assert(realloc_buf[i] == 100 + i);
	}
	for (int i = 4; i < 12; i++) {
		realloc_buf[i] = 200 + i;
	}

	realloc_buf = (int *)realloc(realloc_buf, 6 * sizeof(int));
	assert(realloc_buf != NULL);
	for (int i = 0; i < 4; i++) {
		assert(realloc_buf[i] == 100 + i);
	}
	free(realloc_buf);

	int *new_ptr = (int *)realloc(NULL, 16);
	assert(new_ptr != NULL);
	free(new_ptr);

	free(NULL);

	void *blocks[6];
	for (int i = 0; i < 6; i++) {
		blocks[i] = malloc(1024);
		assert(blocks[i] != NULL);
		memset(blocks[i], 0x5A, 1024);
	}
	for (int i = 0; i < 6; i++) {
		free(blocks[i]);
	}

	log_str("[PASS] heap ecall behavior is correct.");
	while (true) {}
}
