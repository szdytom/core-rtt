#include "corelib.h"

static char *u32_to_hex(char *buf, unsigned long val) {
	static const char hex[] = "0123456789ABCDEF";
	int i;
	char tmp[9];
	for (i = 0; i < 8; i++) {
		tmp[7 - i] = hex[val & 0xF];
		val >>= 4;
	}
	tmp[8] = '\0';
	char *p = tmp;
	while (*p == '0' && *(p + 1)) {
		p++;
	}
	while (*p) {
		*buf++ = *p++;
	}
	return buf;
}

static char *int_to_dec(char *buf, int val) {
	unsigned u;
	char tmp[12];
	int i = 0;
	if (val < 0) {
		*buf++ = '-';
		u = -val;
	} else {
		u = val;
	}
	do {
		tmp[i++] = '0' + (u % 10);
		u /= 10;
	} while (u > 0);
	while (i > 0) {
		*buf++ = tmp[--i];
	}
	return buf;
}

static char *ptr_to_hex(char *buf, const void *ptr) {
	unsigned long addr = (unsigned long)ptr;
	*buf++ = '0';
	*buf++ = 'x';
	return u32_to_hex(buf, addr);
}

char line[64];

int main() {
	{
		extern char _start;
		char *ptr = line;
		strcpy(ptr, "ELF entry point at: ");
		ptr += strlen(ptr);
		ptr = ptr_to_hex(ptr, &_start);
		*ptr++ = '\n';
		log(line, ptr - line);
	}

	{
		extern char __bss_start;
		extern char __BSS_END__;
		char *ptr = line;
		strcpy(ptr, "BSS segment: ");
		ptr += strlen(ptr);
		ptr = ptr_to_hex(ptr, &__bss_start);
		*ptr++ = '-';
		ptr = ptr_to_hex(ptr, &__BSS_END__);
		*ptr++ = '\n';
		log(line, ptr - line);
	}

	{
		char *ptr = line;
		strcpy(ptr, "Some rodata: ");
		ptr += strlen(ptr);
		ptr = ptr_to_hex(ptr, "Hello, World!");
		*ptr++ = '\n';
		log(line, ptr - line);
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
			char *ptr = line;
			strcpy(ptr, "  malloc(10 ints): ");
			ptr += strlen(ptr);
			ptr = ptr_to_hex(ptr, p);
			*ptr++ = '\n';
			log(line, ptr - line);

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
			char *ptr = line;
			strcpy(ptr, "  calloc(10 ints): ");
			ptr += strlen(ptr);
			ptr = ptr_to_hex(ptr, p);
			*ptr++ = '\n';
			log(line, ptr - line);

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
			char *ptr = line;
			strcpy(ptr, "  original: ");
			ptr += strlen(ptr);
			ptr = ptr_to_hex(ptr, p);
			*ptr++ = '\n';
			log(line, ptr - line);

			int *p_new = (int *)realloc(p, 10 * sizeof(int));
			if (p_new) {
				ptr = line;
				strcpy(ptr, "  realloc to 10: ");
				ptr += strlen(ptr);
				ptr = ptr_to_hex(ptr, p_new);
				*ptr++ = '\n';
				log(line, ptr - line);

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
			char *ptr = line;
			strcpy(ptr, "  original: ");
			ptr += strlen(ptr);
			ptr = ptr_to_hex(ptr, p);
			*ptr++ = '\n';
			log(line, ptr - line);

			int *p_new = (int *)realloc(p, 5 * sizeof(int));
			if (p_new) {
				ptr = line;
				strcpy(ptr, "  realloc to 5: ");
				ptr += strlen(ptr);
				ptr = ptr_to_hex(ptr, p_new);
				*ptr++ = '\n';
				log(line, ptr - line);

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
			char *ptr = line;
			strcpy(ptr, "  realloc(NULL) -> ");
			ptr += strlen(ptr);
			ptr = ptr_to_hex(ptr, p);
			*ptr++ = '\n';
			log(line, ptr - line);
			free(p);
		} else {
			log_str("  realloc(NULL) failed\n");
		}
	}

	{
		log_str("Test 6: realloc(ptr, 0)\n");
		int *p = (int *)malloc(10 * sizeof(int));
		if (p) {
			char *ptr = line;
			strcpy(ptr, "  allocated: ");
			ptr += strlen(ptr);
			ptr = ptr_to_hex(ptr, p);
			*ptr++ = '\n';
			log(line, ptr - line);

			int *p_new = (int *)realloc(p, 0);
			ptr = line;
			strcpy(ptr, "  realloc(...,0) -> ");
			ptr += strlen(ptr);
			ptr = ptr_to_hex(ptr, p_new);
			*ptr++ = '\n';
			log(line, ptr - line);

			if (p_new == NULL) {
				log_str("  returned NULL (original freed)\n");
			} else {
				log_str("  returned non-NULL, freeing it...\n");
				free(p_new); // 确保释放
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
				memcpy(ptr, &test_data, sizeof(test_data)); // 写入测试数据
				assert(*(int *)ptr == test_data);
				blocks[count++] = ptr;
				char *p = line;
				strcpy(p, "  block ");
				p += strlen(p);
				p = int_to_dec(p, i + 1);
				strcpy(p, ": ");
				p += 2;
				p = ptr_to_hex(p, ptr);
				*p++ = '\n';
				*p = '\0';
				log(line, p - line);
			} else {
				char *p = line;
				strcpy(p, "  block ");
				p += strlen(p);
				p = int_to_dec(p, i + 1);
				strcpy(p, " failed, heap full\n");
				log(line, p - line);
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
		char *ptr = line;
		strcpy(ptr, "  first alloc: ");
		ptr += strlen(ptr);
		ptr = ptr_to_hex(ptr, p1);
		*ptr++ = '\n';
		log(line, ptr - line);
		free(p1);

		void *p2 = malloc(100);
		ptr = line;
		strcpy(ptr, "  second alloc: ");
		ptr += strlen(ptr);
		ptr = ptr_to_hex(ptr, p2);
		*ptr++ = '\n';
		log(line, ptr - line);
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
