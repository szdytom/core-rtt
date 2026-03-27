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
typedef uint32_t uintptr_t;
typedef int32_t intptr_t;

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
 * @brief Terminate the program abnormally.
 * @note This function does not return. It is a direct wrapper around ecall.
 */
noreturn void abort(void);

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
 * @brief Copy up to n bytes from src to dest, stopping if a null terminator is
 * encountered in src.
 * @param dest Pointer to the destination buffer
 * @param src Pointer to the source string
 * @param n Maximum number of bytes to copy
 * @return Pointer to the destination buffer
 * @note If src is shorter than n bytes, the remainder of dest will be padded
 * with null bytes. If src is longer than n bytes, only n bytes are copied and
 * dest will not be null-terminated. The destination buffer must be large enough
 * to hold at least n bytes.
 */
char *strncpy(char *dest, const char *src, size_t n);

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
 * @brief Allocate a block of memory for an array of nmemb elements of size
 * bytes each, and initialize all bytes in the allocated storage to zero.
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

/**
 * @brief Compute the absolute value of an integer.
 * @param x Integer value
 * @return Absolute value of x
 */
int abs(int x);

/**
 * @brief Compute the absolute value of a long integer.
 * @param x Long integer value
 * @return Absolute value of x
 */
long int labs(long int x);

#define __assert_fail(expr, file, line) \
	(log_str("Assertion failed: " expr ", file " file ", line " #line), abort())

#ifndef NDEBUG
#define assert(expr) \
	((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__))
#else
#define assert(expr) ((void)0)
#endif

struct SensorData {
	bool visible : 1;       // whether this tile is visible
	uint8_t terrain : 2;    // 0: plain, 1: water, 3: obstacle
	bool is_base : 1;       // whether there is a base on this tile
	bool is_resource : 1;   // whether there is a resource zone on this tile
	bool has_unit : 1;      // whether there is a unit on this tile
	bool has_bullet : 1;    // whether there is a bullet on this tile
	bool alliance_unit : 1; // whether the unit on this tile is an allied unit
	                        // (if has_unit is true)
};

/**
 * @brief Get the current turn number.
 * @return Current turn number, starting from 1 and incrementing each tick.
 * @note This function is a direct wrapper around ecall.
 */
int turn(void);

/**
 * @brief Read sensor data for surrounding tiles into the provided buffer.
 * @param data Pointer to a buffer to store the sensor data.
 * @return Number of tiles for which data is written, or a negative error code
 * @note There must be enough space in the buffer to hold the data for all tiles
 * in the vision. Specifically, the buffer size should be at least 25 for base
 * and units without vision upgrade, and at least 81 for units with vision
 * upgrade.
 * This function is a direct wrapper around ecall.
 */
int read_sensor(struct SensorData data[]);

struct DeviceInfo {
	uint8_t id : 5; /**< 0 = base, 1-15 = unit id */
	uint8_t upgrades
		: 3;         /**< bit0=capacity, bit1=vision, bit2=damage; units only */
	uint8_t health;  /**< current health; always 0 for base */
	uint16_t energy; /**< current energy carried */
};

/**
 * @brief Retrieve information about the current device (base or unit).
 * @return A DeviceInfo struct with id, upgrades, health and energy fields.
 * @note For the base, upgrades and health are always zero and energy reflects
 * the base energy pool. For units, id is 1-15. This function is a direct
 * wrapper around ecall.
 */
struct DeviceInfo dev_info(void);

/**
 * @brief Send a message to all allied devices.
 * @param data Pointer to the message payload buffer.
 * @param len Number of bytes to send.
 * @return 0 on success, or a negative error code on failure.
 * @note The length must be between 1 and 512 (inclusive). This function can
 * be called at most once per turn. Messages sent in the current turn are
 * received in the next turn or later.
 * This function is a direct wrapper around ecall.
 */
int send_msg(const uint8_t *data, int len);

/**
 * @brief Receive one message from the incoming queue.
 * @param data Pointer to the destination buffer.
 * @param max_len Maximum number of bytes to write into @p data.
 * @return Number of bytes received, 0 if no message is available, or a
 * negative error code on failure.
 * @note If the next message is larger than @p max_len, only the first
 * @p max_len bytes are copied and the rest is discarded. Passing
 * @p max_len == 0 is valid and skips one queued message without writing to
 * memory.
 * This function is a direct wrapper around ecall.
 */
int recv_msg(uint8_t *data, int max_len);

/**
 * @brief Generate a pseudo-random 32-bit unsigned integer.
 * @return A pseudo-random value in the full uint32_t range.
 * @note The generator state is managed by the runtime and seeded at runtime
 * startup.
 * This function is a direct wrapper around ecall.
 */
uint32_t rand(void);

struct GameInfo {
	uint8_t map_width;    // width of the map in tiles
	uint8_t map_height;   // height of the map in tiles
	uint8_t base_size;    // side length of the square base area in tiles
	uint8_t reserved[13]; // reserved for future use
};

/**
 * @brief Retrieve gamerule and meta information of the current game.
 * @param info Pointer to a GameInfo struct to populate.
 * @return 0 on success, or a negative error code on failure.
 * @note This function is a direct wrapper around ecall.
 */
int meta(struct GameInfo *info);

struct PosInfo {
	uint8_t x;           // x coordinate (column) of the device
	uint8_t y;           // y coordinate (row) of the device
	uint8_t reserved[2]; // reserved for future use
};

/**
 * @brief Get the position of the current device.
 * @return A PosInfo struct with the current x and y coordinates.
 * @note For the base, the returned coordinates are the top-left corner.
 * This function is a direct wrapper around ecall.
 */
struct PosInfo pos(void);

/**
 * @brief Manufacture a new unit with the specified id.
 * @param id Unit id to assign (1-15, must not be in use).
 * @return 0 on success, or a negative error code on failure.
 * @note Base runtime only. This function is a direct wrapper around ecall.
 */
int manufact(int id);

/**
 * @brief Repair the unit with the specified id.
 * @param id Id of the unit to repair.
 * @return 0 on success, or a negative error code on failure.
 * @note Unit must be in the base area. Base runtime only.
 * This function is a direct wrapper around ecall.
 */
int repair(int id);

/**
 * @brief Install an upgrade on the unit with the specified id.
 * @param id Id of the unit to upgrade.
 * @param type Upgrade type: 0=capacity, 1=vision, 2=damage.
 * @return 0 on success, or a negative error code on failure.
 * @note Unit must be in the base area. Base runtime only.
 * This function is a direct wrapper around ecall.
 */
int upgrade(int id, int type);

/**
 * @brief Fire a bullet in the specified direction.
 * @param direction Direction: 0=up, 1=right, 2=down, 3=left.
 * @param power Energy cost deducted from the unit.
 * @return 0 on success, or a negative error code on failure.
 * @note Unit runtime only. 3-turn cooldown after a successful fire.
 * This function is a direct wrapper around ecall.
 */
int fire(int direction, int power);

/**
 * @brief Queue a move in the specified direction for the next turn.
 * @param direction Direction: 0=up, 1=right, 2=down, 3=left.
 * @return 0 on success, or a negative error code on failure.
 * @note If called multiple times per turn only the last call takes effect.
 * Unit runtime only. This function is a direct wrapper around ecall.
 */
int move(int direction);

/**
 * @brief Deposit energy from the unit into the base.
 * @param amount Amount of energy to deposit (must be positive).
 * @return 0 on success, or a negative error code on failure.
 * @note Unit runtime only. Mutually exclusive with withdraw in the same turn.
 * This function is a direct wrapper around ecall.
 */
int deposit(int amount);

/**
 * @brief Withdraw energy from the base into the unit.
 * @param amount Amount of energy to withdraw.
 * @return 0 on success, or a negative error code on failure.
 * @note Unit runtime only. Mutually exclusive with deposit in the same turn.
 * This function is a direct wrapper around ecall.
 */
int withdraw(int amount);

struct MemoryInfo {
	size_t bytes_free;  // free bytes remaining in the managed heap
	size_t bytes_used;  // bytes currently allocated
	size_t chunks_used; // number of live allocation chunks
};

/**
 * @brief Retrieve memory usage statistics for the runtime-managed heap.
 * @param info Pointer to a MemoryInfo struct to populate.
 * @return 0 on success, or a negative error code on failure.
 * @note This function is a direct wrapper around ecall.
 */
int meminfo(struct MemoryInfo *info);

enum {
	DIR_UP = 0,
	DIR_RIGHT = 1,
	DIR_DOWN = 2,
	DIR_LEFT = 3,
};

#define DIR_X_OFFSET_OF(dir) \
	((dir) == DIR_RIGHT ? 1 : ((dir) == DIR_LEFT ? -1 : 0))
#define DIR_Y_OFFSET_OF(dir) \
	((dir) == DIR_DOWN ? 1 : ((dir) == DIR_UP ? -1 : 0))

enum {
	UPGRADE_CAPACITY = 0,
	UPGRADE_VISION = 1,
	UPGRADE_DAMAGE = 2,
};

#define UPGRADE_BIT_OF(upg) (1U << (upg))
#define HAS_UPGRADE(upgrades, upg) (((upgrades) & UPGRADE_BIT_OF(upg)) != 0)

enum {
	TERRAIN_EMPTY = 0,
	TERRAIN_WATER = 1,
	TERRAIN_OBSTACLE = 3,
};

enum {
	EC_OK = 0,
	EC_INVALID_POINTER = -1,
	EC_INVALID_UNIT = -2,
	EC_INSUFFICIENT_ENERGY = -3,
	EC_ON_COOLDOWN = -4,
	EC_CAPACITY_FULL = -5,
	EC_INVALID_ID = -6,
	EC_OUT_OF_RANGE = -7,
	EC_UNSUPPORTED_RUNTIME = -8,
};

/**
 * @brief Format a string into a buffer, similar to snprintf but with only basic
 * %d, %u, %x, %p %c, %s specifiers and no width/precision modifiers.
 * @param buffer Pointer to the destination buffer
 * @param buffer_size Size of the destination buffer in bytes
 * @param fmt Format string containing text and format specifiers
 * @param ... Additional arguments corresponding to the format specifiers
 * @return The number of bytes that would have been written, not including the
 * null terminator.
 */
int fmt_str(char *buffer, size_t buffer_size, const char *fmt, ...);

/**
 * @brief Format a string with a va_list of arguments, similar to vsnprintf but
 * with only basic %d, %u, %x, %p %c, %s specifiers and no width/precision
 * modifiers (va_list version of fmt_str).
 * @param buffer Pointer to the destination buffer
 * @param buffer_size Size of the destination buffer in bytes
 * @param fmt Format string containing text and format specifiers
 * @param args va_list of additional arguments corresponding to the format
 * specifiers
 * @return The number of bytes that would have been written, not including the
 * null terminator.
 */
int vfmt_str(char *buffer, size_t buffer_size, const char *fmt, va_list args);

/**
 * @brief A printf-like function that formats a string and logs it using the
 * runtime's logging system.
 * @param fmt Format string containing text and format specifiers
 * @param ... Additional arguments corresponding to the format specifiers
 * @return The number of bytes that would have been logged, not including the
 * null terminator
 * @note This function internally uses fmt_str to format the string, and then
 * logs it using log_str. The maximum length of the formatted string is 512
 * bytes (same as a single ecall log message).
 */
int logf(const char *fmt, ...);

#endif // CORELIB_H
