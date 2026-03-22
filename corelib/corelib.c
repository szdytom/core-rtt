#include "corelib.h"

int log(const char *str, int n) {
	register int call_id asm("a7") = 0x01;
	register int arg0 asm("a0") = (int)str;
	register int arg1 asm("a1") = n;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id), "r"(arg1));
	return arg0;
}

int log_str(const char *str) {
	return log(str, strlen(str));
}

static char* write_unsigned_decimal(char *p, char *end, unsigned int val) {
	if (val == 0) {
		if (p < end) {
			*p++ = '0';
		}
		return p;
	}

	int len = 0, tmp = val;
	while (tmp > 0) {
		len++;
		tmp /= 10;
	}

	for (int i = len - 1; i >= 0; i--) {
		if (p + i < end) {
			p[i] = '0' + (val % 10);
		}
		val /= 10;
	}
	return p + len;
}

int fmt_str(char *buffer, size_t buffer_size, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int len = vfmt_str(buffer, buffer_size, fmt, args);
	va_end(args);
	return len;
}

int vfmt_str(char *buffer, size_t buffer_size, const char *fmt, va_list args) {
	char *p = buffer;
	char *end = buffer + buffer_size - 1; // reserve space for null terminator
	for (const char *f = fmt; *f && p < end; f++) {
		if (*f != '%') {
			*p++ = *f;
			continue;
		}

		f++;
		if (*f == '\0') {
			// Format string ends with a single '%', treat it as literal '%'
			*p++ = '%';
			break;
		}

		if (*f == '%') {
			*p++ = '%';
			continue;
		}

		if (*f == 's') {
			const char *val = va_arg(args, const char *);
			while (*val && p < end) {
				*p++ = *val++;
			}
			continue;
		}

		if (*f == 'c') {
			int val = va_arg(args, int);
			*p++ = (char)val;
			continue;
		}

		if (*f == 'd') {
			int val = va_arg(args, int);
			if (val < 0) {
				*p++ = '-';
				val = -val;
			}
			p = write_unsigned_decimal(p, end, (unsigned int)val);
		} else if (*f == 'u') {
			unsigned int val = va_arg(args, unsigned int);
			p = write_unsigned_decimal(p, end, val);
		} else if (*f == 'x') {
			unsigned int val = va_arg(args, unsigned int);
			if (val == 0) {
				if (p < end) {
					*p++ = '0';
				}
				continue;
			}

			int len = 0, tmp = val;
			while (tmp > 0) {
				len++;
				tmp /= 16;
			}

			for (int i = len - 1; i >= 0; i--) {
				if (p + i < end) {
					int digit = val % 16;
					p[i] = digit < 10 ? '0' + digit : 'a' + (digit - 10);
				}
				val /= 16;
			}
			p += len;
		} else if (*f == 'p') {
			uintptr_t val = (uintptr_t)va_arg(args, void *);
			// %p has 0x prefix and is zero-padded to pointer width (8 hex digits for 32-bit)
			if (p < end) {
				*p++ = '0';
			}
			if (p < end) {
				*p++ = 'x';
			}

			for (int i = 7; i >= 0; i--) {
				if (p + i < end) {
					int digit = (val >> (i * 4)) & 0xF;
					p[i] = digit < 10 ? '0' + digit : 'a' + (digit - 10);
				}
			}
			p += 8;
		} else {
			// Unsupported format specifier, treat it as literal
			if (p < end) {
				*p++ = '%';
			}
			if (p < end) {
				*p++ = *f;
			}
		}

		if (p >= end) {
			p = end;
			break;
		}
	}

	*p = '\0'; // null terminate the string
	return p - buffer; // return the number of bytes written
}

int logf(const char *fmt, ...) {
	// The ecall log message size limit is 512 bytes, but it don't require a null terminator.
	// The fmt_str function will ensure that the formatted string is null-terminated and does not exceed the buffer size, so need to reserve 1 byte for null terminator.
	char buffer[513];
	va_list args;
	va_start(args, fmt);
	int len = vfmt_str(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	if (len > 512) {
		len = 512; // truncate to maximum log message size
	}
	if (len > 0) {
		return log(buffer, len);
	}
	return 0;
}

int strlen(const char *str) {
	int len = 0;
	while (str[len] != '\0') {
		len++;
	}
	return len;
}

char *strcpy(char *dest, const char *src) {
	char *ret = dest;
	while ((*dest++ = *src++) != '\0') {}
	return ret;
}

void *memcpy(void *dest, const void *src, size_t n) {
	unsigned char *d = (unsigned char *)dest;
	const unsigned char *s = (const unsigned char *)src;
	for (size_t i = 0; i < n; i++) {
		d[i] = s[i];
	}
	return dest;
}

void *memset(void *dest, int value, size_t n) {
	unsigned char *d = (unsigned char *)dest;
	unsigned char val = (unsigned char)value;
	for (size_t i = 0; i < n; i++) {
		d[i] = val;
	}
	return dest;
}

int memcmp(const void *buf1, const void *buf2, size_t n) {
	const unsigned char *b1 = (const unsigned char *)buf1;
	const unsigned char *b2 = (const unsigned char *)buf2;
	for (size_t i = 0; i < n; i++) {
		if (b1[i] != b2[i]) {
			return (int)b1[i] - (int)b2[i];
		}
	}
	return 0;
}

int strcmp(const char *str1, const char *str2) {
	while (*str1 && (*str1 == *str2)) {
		str1++;
		str2++;
	}
	return *str1 - *str2;
}

int strncmp(const char *str1, const char *str2, size_t n) {
	for (size_t i = 0; i < n; i++) {
		if (str1[i] != str2[i] || str1[i] == '\0') {
			return str1[i] - str2[i];
		}
	}
	return 0;
}

void *malloc(size_t size) {
	register int call_id asm("a7") = 0x40;
	register int arg0 asm("a0") = (int)size;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id));
	return (void *)arg0;
}

void *calloc(size_t nmemb, size_t size) {
	register int call_id asm("a7") = 0x41;
	register int arg0 asm("a0") = (int)nmemb;
	register int arg1 asm("a1") = (int)size;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id), "r"(arg1));
	return (void *)arg0;
}

void *realloc(void *ptr, size_t size) {
	register int call_id asm("a7") = 0x42;
	register int arg0 asm("a0") = (int)ptr;
	register int arg1 asm("a1") = (int)size;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id), "r"(arg1));
	return (void *)arg0;
}

void free(void *ptr) {
	register int call_id asm("a7") = 0x43;
	register int arg0 asm("a0") = (int)ptr;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id));
}

int isalnum(int c) {
	return isalpha(c) || isdigit(c);
}

int isalpha(int c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int isblank(int c) {
	return c == ' ' || c == '\t';
}

int iscntrl(int c) {
	return (c >= 0 && c <= 31) || c == 127;
}

int isdigit(int c) {
	return c >= '0' && c <= '9';
}

int isgraph(int c) {
	return c >= '!' && c <= '~';
}

int islower(int c) {
	return c >= 'a' && c <= 'z';
}

int isprint(int c) {
	return c >= ' ' && c <= '~';
}

int ispunct(int c) {
	return isgraph(c) && !isalnum(c);
}

int isspace(int c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'
		|| c == '\v';
}

int isupper(int c) {
	return c >= 'A' && c <= 'Z';
}

int isxdigit(int c) {
	return isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int tolower(int c) {
	if (isupper(c)) {
		return c + ('a' - 'A');
	}
	return c;
}

int toupper(int c) {
	if (islower(c)) {
		return c - ('a' - 'A');
	}
	return c;
}

int abs(int x) {
	return x < 0 ? -x : x;
}

long int labs(long int x) {
	return x < 0 ? -x : x;
}

void _start() {
	// First, configure the global pointer
	asm volatile(".option push 				\t\n\
	 .option norelax 			\t\n\
	 1:auipc gp, %pcrel_hi(__global_pointer$) \t\n\
	 addi  gp, gp, %pcrel_lo(1b) \t\n\
	.option pop					\t\n\
");

	// make sure all accesses to static memory happen after:
	asm volatile("" : : : "memory");

	// Clear the .bss segment
	extern char __bss_start;
	extern char __BSS_END__;
	for (char *bss = &__bss_start; bss < &__BSS_END__; bss++) {
		*bss = 0;
	}

	// Call the global constructors
	// Not actually useful for C
	// But we might link C++ and others to this
	extern void (*__init_array_start[])();
	extern void (*__init_array_end[])();
	int count = __init_array_end - __init_array_start;
	for (int i = 0; i < count; i++) {
		__init_array_start[i]();
	}

	extern int main();
	main();

	// Should never reach here
	// __builtin_unreachable();
	abort();
}

int turn() {
	register int call_id asm("a7") = 0x10;
	register int arg0 asm("a0");
	asm volatile("ecall" : "=r"(arg0) : "r"(call_id));
	return arg0;
}

int read_sensor(struct SensorData data[]) {
	register int call_id asm("a7") = 0x12;
	register int arg0 asm("a0") = (int)data;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id) : "memory");
	return arg0;
}

struct DeviceInfo dev_info() {
	register int call_id asm("a7") = 0x11;
	register uint32_t raw asm("a0");
	asm volatile("ecall" : "=r"(raw) : "r"(call_id));
	// Reinterpret the packed 32-bit return value as the DeviceInfo struct.
	union { uint32_t r; struct DeviceInfo d; } u;
	u.r = raw;
	return u.d;
}

int send_msg(const uint8_t *data, int len) {
	register int call_id asm("a7") = 0x14;
	register int arg0 asm("a0") = (int)data;
	register int arg1 asm("a1") = len;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id), "r"(arg1));
	return arg0;
}

int recv_msg(uint8_t *data, int max_len) {
	register int call_id asm("a7") = 0x13;
	register int arg0 asm("a0") = (int)data;
	register int arg1 asm("a1") = max_len;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id), "r"(arg1) : "memory");
	return arg0;
}

uint32_t rand() {
	register int call_id asm("a7") = 0x4F;
	register uint32_t arg0 asm("a0");
	asm volatile("ecall" : "=r"(arg0) : "r"(call_id));
	return arg0;
}

int meta(struct GameInfo *info) {
	register int call_id asm("a7") = 0x0F;
	register int arg0 asm("a0") = (int)info;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id) : "memory");
	return arg0;
}

struct PosInfo pos() {
	register int call_id asm("a7") = 0x15;
	register uint32_t raw asm("a0");
	asm volatile("ecall" : "=r"(raw) : "r"(call_id));
	union { uint32_t r; struct PosInfo p; } u;
	u.r = raw;
	return u.p;
}

int manufact(int id) {
	register int call_id asm("a7") = 0x20;
	register int arg0 asm("a0") = id;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id));
	return arg0;
}

int repair(int id) {
	register int call_id asm("a7") = 0x21;
	register int arg0 asm("a0") = id;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id));
	return arg0;
}

int upgrade(int id, int type) {
	register int call_id asm("a7") = 0x22;
	register int arg0 asm("a0") = id;
	register int arg1 asm("a1") = type;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id), "r"(arg1));
	return arg0;
}

int fire(int direction, int power) {
	register int call_id asm("a7") = 0x30;
	register int arg0 asm("a0") = direction;
	register int arg1 asm("a1") = power;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id), "r"(arg1));
	return arg0;
}

int move(int direction) {
	register int call_id asm("a7") = 0x31;
	register int arg0 asm("a0") = direction;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id));
	return arg0;
}

int deposit(int amount) {
	register int call_id asm("a7") = 0x32;
	register int arg0 asm("a0") = amount;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id));
	return arg0;
}

int withdraw(int amount) {
	register int call_id asm("a7") = 0x33;
	register int arg0 asm("a0") = amount;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id));
	return arg0;
}

int meminfo(struct MemoryInfo *info) {
	register int call_id asm("a7") = 0x44;
	register int arg0 asm("a0") = (int)info;
	asm volatile("ecall" : "+r"(arg0) : "r"(call_id) : "memory");
	return arg0;
}

noreturn void abort() {
	register int call_id asm("a7") = 0x00;
	asm volatile("ecall" : : "r"(call_id));
	__builtin_unreachable();
}
