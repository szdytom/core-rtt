# Corelib Documentation

The corelib is a support library that provides essential functionalities for the guest programs. It's mainly a hybrid of wrappers around the runtime's environment calls and some free-standing utilities from the standard C library. The corelib is designed to be minimal and efficient, ensuring that it can be easily integrated into various guest environments without unnecessary overhead.

## Integration

To integrate the corelib into your guest program, you can include the corelib header and link against the corelib library. More specifically, you can find the header file at `corelib/corelib.h`. Executing `make` in the `corelib` directory with a valid RISC-V 32-bit toolchain installed will build the corelib and generate `libcorelib.a`, which can be linked against your guest program.

The `libcorelib.a` is built with `-ffunction-sections` and `-fdata-sections` flags, which allows for dead code elimination when linking. You can use the `--gc-sections` flag with your linker to ensure that only the necessary parts of the corelib are included in your final binary, reducing the overall size. Note: the flag is `-Wl,--gc-sections` when using the C compiler to link.

The corelib is built for the RISC-V architecture with the `-march=rv32gv_zba_zbb_zbc_zbs` and `-mabi=ilp32d` flags, which means it uses the RV32GCB instruction set and the ILP32D ABI. Make sure to compile your guest program with compatible flags to ensure proper integration with the corelib, or if you are using a different architecture or ABI, you need to build the corelib from source yourself with the appropriate flags.

## Functionality

The corelib provides a range of functionalities.

### Integral Types and Limits 

The corelib defines the following integral types (same as C standard library):

```
uint8_t
uint16_t
uint32_t
int8_t
int16_t
int32_t
size_t
isize_t
```

The following limit macros are also defined:

```
INT8_MIN
INT16_MIN
INT32_MIN
INT8_MAX
INT16_MAX
INT32_MAX
UINT8_MAX
UINT16_MAX
UINT32_MAX
SIZE_WIDTH
SIZE_MAX
CHAR_MIN
CHAR_MAX
SCHAR_MIN
SCHAR_MAX
INT_MIN
INT_MAX
LONG_MIN
LONG_MAX
```

Additionally, `CHAR_BIT` macro is defined as 8.

### Boolean Type

The corelib defines a boolean type `bool` and the corresponding values `true` and `false`. The test macro `__bool_true_false_are_defined` is defined to 1.

### Memory Management

The corelib defines the `NULL` macro as a null pointer constant.

Memory allocation functions such as `malloc` are defined as well, refer to the [Environment Call Wrappers](#environment-call-wrappers) section for more details.

### Type Operators

The corelib provides `offsetof`, `typeof`, `alignof`, and `alignas` operators for working with types and their properties. The test macro `__alignas_is_defined` and `__alignof_is_defined` are defined to 1.

### Untility and Algorithmic Functions

The corelib provides the following utility functions:

```c
int abs(int x);
long labs(long x);
```

Futher utility functions, such as `div` and `ldiv`, may be added in the future, if many user programs have demands for them.

### Attribute Macros

The corelib defines the `noreturn` attribute macro, which can be used to indicate that a function does not return.

### Variable Argument Lists

The corelib provides support for variable argument lists using the `va_list`, `va_start`, `va_arg`, `va_end`, and `va_copy` macros.

### String Functions

The following string functions are defined. The working of these functions is straightforward and similar to their standard C library counterparts.

```c
int strlen(const char *s);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
```

Further string functions, such as `strcoll`, may be added in the future, if many user programs have demands for them.

### Assertion Macro

The corelib defines the `assert` macro, which can be used to perform runtime assertions. If the expression passed to `assert` evaluates to false, the program will print an error message including the expression, file name, and line number, and then abort execution.

### Character Types

The corelib provides the following functions:

```c
int isalnum(int c);
int isalpha(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);
int tolower(int c);
int toupper(int c);
```

It is worth noting that these functions are implemented using simple character checks and do not rely on locale settings, i.e., they only work with ASCII characters. The behavior of these functions for non-ASCII characters is undefined.

### Environment Call Wrappers

The corelib provides wrappers around the runtime's environment calls. These structures are defined

```c
struct GameInfo {
	uint8_t map_width;
	uint8_t map_height;
	uint8_t base_size;
	uint8_t reserved[13];
};

struct DeviceInfo {
	uint8_t id : 5;
	uint8_t upgrades : 3;
	uint8_t health;
	uint16_t energy;
};

struct SensorData {
	bool visible : 1;
	uint8_t terrain : 2;
	bool is_base : 1;
	bool is_resource : 1;
	bool has_unit : 1;
	bool has_bullet : 1;
	bool alliance_unit : 1;
};

struct PosInfo {
	uint8_t x;
	uint8_t y;
	uint8_t reserved[2];
};

struct MemoryInfo {
	size_t bytes_free;
	size_t bytes_used;
	size_t chunks_used;
};
```

Please note that these structures are not `typedef`ed, so you need to use `struct GameInfo`, `struct DeviceInfo`, etc., when declaring variables of these types. This design choice is intentional, as all those structures' fields are public and can be directly accessed by the user program, so there is no need to hide the implementation details behind a typedef.

The corelib provides the following enums:

```c
enum {
	DIR_UP = 0,
	DIR_RIGHT = 1,
	DIR_DOWN = 2,
	DIR_LEFT = 3,
};

enum {
	UPGRADE_CAPACITY = 0,
	UPGRADE_VISION = 1,
	UPGRADE_DAMAGE = 2,
};

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
```

The corelib defines the following macros for conviniently dealing with these enums:

```c
DIR_X_OFFSET_OF(dir) // Evaluates to the x offset of the given direction. For example, DIR_X_OFFSET_OF(DIR_RIGHT) will evaluate to 1, while DIR_X_OFFSET_OF(DIR_LEFT) will evaluate to -1.
DIR_Y_OFFSET_OF(dir) // Evaluates to the y offset of the given direction. For example, DIR_Y_OFFSET_OF(DIR_DOWN) will evaluate to 1, while DIR_Y_OFFSET_OF(DIR_UP) will evaluate to -1.
UPGRADE_BIT_OF(upg) // Evaluates to the bit mask of the given upgrade. For example, UPGRADE_BIT_OF(UPGRADE_VISION) will evaluate to 0b10.
HAS_UPGRADE(upgrades, upg) // Evaluates to whether the given upgrades bitmask contains the given upgrade. For example, HAS_UPGRADE(0b101, UPGRADE_DAMAGE) will evaluate to true, while HAS_UPGRADE(0b101, UPGRADE_VISION) will evaluate to false.
```

The corelib provides the following functions to interact with the runtime environment:

```c
noreturn void abort();
int log(const char *str, int len);
int meta(struct GameInfo *game_info);
int turn();
struct DeviceInfo dev_info();
int read_sensor(struct SensorData data[]);
int recv_msg(uint8_t buf[], int len);
int send_msg(const uint8_t buf[], int len);
struct PosInfo pos();
int manufact(int id);
int repair(int id);
int upgrade(int id, int type);
int fire(int direction, int power);
int move(int direction);
int deposit(int amount);
int withdraw(int amount);
void *malloc(size_t size);
void *calloc(size_t num, size_t size);
void *realloc(void *ptr, size_t new_size);
void free(void *ptr);
int meminfo(struct MemoryInfo *mem_info);
uint32_t rand();
```

Futhermore, the corelib provides the following convinience functions:

```c
log_str(const char *str); // Logs a null-terminated string. Automatically calculates the length of the string and calls log().
```

## Entry Point

The runtime expects the guest program to define a function `_start`, which serves as the entry point of the program. The `_start` function is provided by the corelib, and it will take care of initializing the register and memory state before calling the user-defined `main` function. Therefore, you only need to define a `main` function in your guest program, and the corelib will handle the rest.

```c
int main();
```

The `main` function should not return a value under normal circumstances. If the `main` function returns, the corelib will call `abort()` to terminate the program.
