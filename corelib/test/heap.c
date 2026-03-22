#include "corelib.h"

int main() {
	{
		extern char _start;
		logf("ELF entry point at: %p\n", &_start);
	}

	{
		extern char __bss_start;
		extern char __BSS_END__;
		logf("BSS segment: %p-%p\n", &__bss_start, &__BSS_END__);
	}

	{
		logf("Some rodata: %p\n", "Hello, World!");
	}

	log_str("========== Memory Allocator Test ==========\n");

	// Test 0: R/W to somewhere not allocated should be fine
	{
		log_str("Test 0: R/W to unallocated memory\n");
		int *ptr = (int *)0x12345600;
		*ptr = 0xAAAAAAAA; // Automatically maps on page fault
		assert(*ptr == 0xAAAAAAAA);
		log_str("  R/W to unallocated memory works (page fault handled)\n");
	}

	{
		log_str("Test 1: malloc & free\n");
		int *p = (int *)malloc(10 * sizeof(int));
		if (p) {
			logf("  malloc(10 ints): %p\n", p);

			for (int i = 0; i < 10; i++) {
				p[i] = i;
			}
			int ok = 1;
			for (int i = 0; i < 10; i++) {
				if (p[i] != i) {
					ok = 0;
				}
			}
			log_str(ok ? "  data verified\n" : "  data mismatch!\n");

			free(p);
			log_str("  freed\n");
		} else {
			log_str("  malloc failed\n");
		}
	}

	{
		log_str("Test 2: calloc\n");
		int *p = (int *)calloc(10, sizeof(int));
		if (p) {
			logf("  calloc(10 ints): %p\n", p);

			int ok = 1;
			for (int i = 0; i < 10; i++) {
				if (p[i] != 0) {
					ok = 0;
				}
			}
			log_str(ok ? "  zero-initialized\n" : "  not zero!\n");

			free(p);
		} else {
			log_str("  calloc failed\n");
		}
	}

	{
		log_str("Test 3: realloc (expand)\n");
		int *p = (int *)malloc(5 * sizeof(int));
		if (p) {
			for (int i = 0; i < 5; i++) {
				p[i] = i;
			}
			logf("  original: %p\n", p);

			int *p_new = (int *)realloc(p, 10 * sizeof(int));
			if (p_new) {
				logf("  realloc to 10: %p\n", p_new);

				int ok = 1;
				for (int i = 0; i < 5; i++) {
					if (p_new[i] != i) {
						ok = 0;
					}
				}
				log_str(ok ? "  data preserved\n" : "  data lost!\n");

				for (int i = 5; i < 10; i++) {
					p_new[i] = i;
				}
				free(p_new);
			} else {
				log_str("  realloc failed, freeing original\n");
				free(p);
			}
		} else {
			log_str("  initial malloc failed\n");
		}
	}

	{
		log_str("Test 4: realloc (shrink)\n");
		int *p = (int *)malloc(10 * sizeof(int));
		if (p) {
			for (int i = 0; i < 10; i++) {
				p[i] = i;
			}
			logf("  original: %p\n", p);

			int *p_new = (int *)realloc(p, 5 * sizeof(int));
			if (p_new) {
				logf("  realloc to 5: %p\n", p_new);

				int ok = 1;
				for (int i = 0; i < 5; i++) {
					if (p_new[i] != i) {
						ok = 0;
					}
				}
				log_str(ok ? "  data preserved\n" : "  data lost!\n");

				free(p_new);
			} else {
				log_str("  realloc failed, freeing original\n");
				free(p);
			}
		} else {
			log_str("  initial malloc failed\n");
		}
	}

	{
		log_str("Test 5: realloc(NULL, size)\n");
		int *p = (int *)realloc(NULL, 10 * sizeof(int));
		if (p) {
			logf("  realloc(NULL) -> %p\n", p);
			free(p);
		} else {
			log_str("  realloc(NULL) failed\n");
		}
	}

	{
		log_str("Test 6: realloc(ptr, 0)\n");
		int *p = (int *)malloc(10 * sizeof(int));
		if (p) {
			logf("  allocated: %p\n", p);

			int *p_new = (int *)realloc(p, 0);
			logf("  realloc(...,0) -> %p\n", p_new);

			if (p_new == NULL) {
				log_str("  returned NULL (original freed)\n");
			} else {
				log_str("  returned non-NULL, freeing it...\n");
				free(p_new);
			}
		} else {
			log_str("  malloc failed\n");
		}
	}

	{
		log_str("Test 7: free(NULL)\n");
		free(NULL);
		log_str("  free(NULL) called (should be safe)\n");
	}

	// Allocate 80KB in total
	{
		log_str("Test 8: allocate stress\n");
		int test_data = 0xAAAAAAAA;
		void *blocks[20];
		int count = 0;
		for (int i = 0; i < 20; i++) {
			void *ptr = malloc(4096);
			if (ptr) {
				memcpy(ptr, &test_data, sizeof(test_data));
				assert(*(int *)ptr == test_data);
				blocks[count++] = ptr;
				logf("  block %d: %p\n", i + 1, ptr);
			} else {
				logf("  block %d failed, heap full\n", i + 1);
				break;
			}
		}

		asm("ebreak");
		for (int i = 0; i < count; i++) {
			free(blocks[i]);
		}
		log_str("  freed all allocated blocks\n");
	}

	{
		log_str("Test 9: reuse after free\n");
		void *p1 = malloc(100);
		logf("  first alloc: %p\n", p1);
		free(p1);

		void *p2 = malloc(100);
		logf("  second alloc: %p\n", p2);
		if (p1 == p2) {
			log_str("  same address, good reuse\n");
		} else {
			log_str("  different address (still OK)\n");
		}
		free(p2);
	}

	log_str("========== All tests completed ==========\n");

	while (1) {}
	return 0;
}
