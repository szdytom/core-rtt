#ifndef CORELIB_H
#define CORELIB_H

// Basic freestanding facilities and bindings to the runtime.

// RISC-V 32-bit configuration
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;

typedef uint32_t size_t;
typedef int32_t isize_t;

#define NULL ((void *)0)
#define bool _Bool
#define false 0
#define true 1
#define __bool_true_false_are_defined 1

#define offsetof(type, member) ((size_t)&(((type *)0)->member))
#define typeof __builtin_typeof

#define alignas _Alignas
#define alignof _Alignof
#define __alignas_is_defined 1
#define __alignof_is_defined 1

#define noreturn _Noreturn

#define INT8_MIN (-128)
#define INT16_MIN (-32768)
#define INT32_MIN (-2147483648)
#define INT8_MAX (127)
#define INT16_MAX (32767)
#define INT32_MAX (2147483647)

#define UINT8_MAX (255)
#define UINT16_MAX (65535)
#define UINT32_MAX (4294967295U)

#define SIZE_WIDTH 32
#define SIZE_MAX UINT32_MAX

#define CHAR_BIT 8
#define CHAR_MIN 0
#define CHAR_MAX 255
#define SCHAR_MIN (-128)
#define SCHAR_MAX (127)
#define INT_MIN (-2147483648)
#define INT_MAX (2147483647)
#define LONG_MIN (-2147483648L)
#define LONG_MAX (2147483647L)

typedef __builtin_va_list va_list;

#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v) __builtin_va_end(v)
#define va_arg(v, l) __builtin_va_arg(v, l)
#define va_copy(d, s) __builtin_va_copy(d, s)

/**
 * @brief Log a string to the runtime's logging system.
 * @param str Pointer to the string to log
 * @param n Length of the string in bytes
 * @return Number of bytes successfully logged
 * @note The string does not need to be null-terminated, and can contain null
 * bytes. This function is a direct wrapper around ecall.
 */
int log(const char *str, int n);

/**
 * @brief Log a null-terminated string to the runtime's logging system.
 * @param str Pointer to the null-terminated string to log
 * @return Number of bytes successfully logged
 */
int log_str(const char *str);

/**
 * @brief Get the length of a null-terminated string.
 * @param str Pointer to the null-terminated string
 * @return Length of the string in bytes, not including the null terminator
 */
int strlen(const char *str);

/**
 * @brief Copy a null-terminated string from src to dest.
 * @param dest Pointer to the destination buffer
 * @param src Pointer to the null-terminated source string
 * @return Pointer to the destination buffer
 * @note The destination buffer must be large enough to hold the source string
 * including the null terminator.
 */
char *strcpy(char *dest, const char *src);

/**
 * @brief Copy a specified number of bytes from src to dest.
 * @param dest Pointer to the destination buffer
 * @param src Pointer to the source buffer
 * @param n Number of bytes to copy
 * @return Pointer to the destination buffer
 */
void *memcpy(void *dest, const void *src, size_t n);

/**
 * @brief Set a specified number of bytes in dest to a given value.
 * @param dest Pointer to the destination buffer
 * @param value Value to set (converted to unsigned char)
 * @param n Number of bytes to set
 * @return Pointer to the destination buffer
 */
void *memset(void *dest, int value, size_t n);

/**
 * @brief Compare two buffers byte by byte.
 * @param buf1 Pointer to the first buffer
 * @param buf2 Pointer to the second buffer
 * @param n Number of bytes to compare
 * @return An integer less than, equal to, or greater than zero if the first n
 * bytes of buf1 are found, respectively, to be less than, to match, or be
 * greater than the first n bytes of buf2.
 */
int memcmp(const void *buf1, const void *buf2, size_t n);

/**
 * @brief Compare two null-terminated strings.
 * @param str1 Pointer to the first null-terminated string
 * @param str2 Pointer to the second null-terminated string
 * @return An integer less than, equal to, or greater than zero if str1 is
 * found, respectively, to be less than, to match, or be greater than str2
 * in lexicographical order.
 */
int strcmp(const char *str1, const char *str2);

/**
 * @brief Compare up to n bytes of two strings.
 * @param str1 Pointer to the first null-terminated string
 * @param str2 Pointer to the second null-terminated string
 * @param n Maximum number of bytes to compare
 * @return An integer less than, equal to, or greater than zero if the first n
 * bytes of str1 are found, respectively, to be less than, to match, or be
 * greater than the first n bytes of str2 in lexicographical order.
 */
int strncmp(const char *str1, const char *str2, size_t n);

// TODO: strcoll, strchr, strrchr, strspn, strcspn, strtok, strcat
// TODO: memchr, memmove

/**
 * @brief Allocate a block of memory of the specified size.
 * @param size Number of bytes to allocate
 * @return Pointer to the allocated memory, or NULL if allocation fails
 * @note The allocated memory is not initialized. This function is a direct
 * wrapper around ecall.
 */
void *malloc(size_t size);

/**
 * @brief Allocate a block of memory for an array of nmemb elements of size bytes
 * each, and initialize all bytes in the allocated storage to zero.
 * @param nmemb Number of elements to allocate
 * @param size Size of each element in bytes
 * @return Pointer to the allocated memory, or NULL if allocation fails
 * @note This function is a direct wrapper around ecall.
 */
void *calloc(size_t nmemb, size_t size);

/**
 * @brief Change the size of the memory block pointed to by ptr to size bytes.
 * @param ptr Pointer to the memory block to resize
 * @param size New size of the memory block in bytes
 * @return Pointer to the reallocated memory, which may be the same as ptr or
 * a new location, or NULL if reallocation fails. If size is zero and ptr is
 * not NULL, the memory block is freed and NULL is returned.
 * @note This function is a direct wrapper around ecall.
 */
void *realloc(void *ptr, size_t size);

/**
 * @brief Deallocate a block of memory previously allocated by malloc.
 * @param ptr Pointer to the memory block to deallocate
 * @note If ptr is NULL, no operation is performed. This function is a direct
 * wrapper around ecall.
 */
void free(void *ptr);

/**
 * @brief Check if character is alphanumeric (letter or digit).
 * @param c Character to check (as int)
 * @return Non-zero if c is alphanumeric, zero otherwise
 */
int isalnum(int c);

/**
 * @brief Check if character is alphabetic (letter).
 * @param c Character to check (as int)
 * @return Non-zero if c is alphabetic, zero otherwise
 */
int isalpha(int c);

/**
 * @brief Check if character is blank (space or tab).
 * @param c Character to check (as int)
 * @return Non-zero if c is blank, zero otherwise
 */
int isblank(int c);

/**
 * @brief Check if character is a control character.
 * @param c Character to check (as int)
 * @return Non-zero if c is a control character, zero otherwise
 */
int iscntrl(int c);

/**
 * @brief Check if character is a decimal digit (0-9).
 * @param c Character to check (as int)
 * @return Non-zero if c is a digit, zero otherwise
 */
int isdigit(int c);

/**
 * @brief Check if character is graphical (has visible representation).
 * @param c Character to check (as int)
 * @return Non-zero if c is graphical, zero otherwise
 */
int isgraph(int c);

/**
 * @brief Check if character is lowercase letter.
 * @param c Character to check (as int)
 * @return Non-zero if c is lowercase, zero otherwise
 */
int islower(int c);

/**
 * @brief Check if character is printable (including space).
 * @param c Character to check (as int)
 * @return Non-zero if c is printable, zero otherwise
 */
int isprint(int c);

/**
 * @brief Check if character is punctuation.
 * @param c Character to check (as int)
 * @return Non-zero if c is punctuation, zero otherwise
 */
int ispunct(int c);

/**
 * @brief Check if character is whitespace.
 * @param c Character to check (as int)
 * @return Non-zero if c is whitespace, zero otherwise
 */
int isspace(int c);

/**
 * @brief Check if character is uppercase letter.
 * @param c Character to check (as int)
 * @return Non-zero if c is uppercase, zero otherwise
 */
int isupper(int c);

/**
 * @brief Check if character is hexadecimal digit (0-9, A-F, a-f).
 * @param c Character to check (as int)
 * @return Non-zero if c is hexadecimal digit, zero otherwise
 */
int isxdigit(int c);

/**
 * @brief Convert uppercase letter to lowercase.
 * @param c Character to convert (as int)
 * @return Lowercase version of c if c is uppercase, otherwise c unchanged
 */
int tolower(int c);

/**
 * @brief Convert lowercase letter to uppercase.
 * @param c Character to convert (as int)
 * @return Uppercase version of c if c is lowercase, otherwise c unchanged
 */
int toupper(int c);

#define __assert_fail(expr, file, line)                     \
	(void)log_str(                                                \
		"Assertion failed: " expr ", file " file ", line " #line \
	)

#ifndef NDEBUG
#define assert(expr) \
	((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__))
#else
#define assert(expr) ((void)0)
#endif

struct SensorData {
	bool visible : 1;		// whether this tile is visible
	uint8_t terrain : 2; 	// 0: plain, 1: water, 3: obstacle
	bool is_base : 1;		// whether there is a base on this tile
	bool is_resource : 1;	// whether there is a resource zone on this tile
	bool has_unit : 1;		// whether there is a unit on this tile
	bool has_bullet : 1;	// whether there is a bullet on this tile
	bool alliance_unit : 1;	// whether the unit on this tile is an allied unit (if has_unit is true)
};

int read_sensor(struct SensorData data[]);

#endif // CORELIB_H
