# Core RTT API Specification

Version: 0.1

## Scope

This document describes the API for the game *Core RTT*. It is intended for all players to write their own tactics and strategies for the game.

## Sandbox

The game emulates player's programs in a RISC-V sandbox. The sandbox supports 32-bit RISC-V instructions of RV32GCB(imafdc_zicsr_zifence_zicond_zba_zbb_zbc_zbs). The sandbox can be considered as a completely freestanding RISC-V environment, which means that there is no operating system or standard library available. The only way for the player's program to interact with the game world is through the provided environment calls.

The default stack pointer (sp) is set to zero. Generally, the program need to be compiled with `-nostdlib -nostdinc -ffreestanding` flags.

The game accepts ELF binaries as the player's program. The entry point of the program is a C function named `_start`.

Note that in languages like C++, the name mangling may change the name of the entry point. To avoid this, you can use `extern "C"` to declare the `_start` function (and `#[no_mangle]` in Rust).

The player's program should configure the global pointer, initialize .bss section, and invoke initialization routines by itself if needed.

The program should never return from the `_start` function. The behavior is undefined if the program returns from `_start`. (In general, the game will restart the player's program at the next tick.)

## Memory

The sandbox provides a mapped memory space for the player's program. The memory is byte-addressable and has a maximum size of 128KB. The memory is divided into pages of 4KB each.

The memory limit includes both the code and data of the program. Page over 0x0 is allocated but disabled for catching null pointer dereference. So please do expect that the actual memory available to the program is less than 128KB.

The memory is divided into two regions: the flat quick-access region and the paged region. The flat quick-access region is a contiguous memory region of 64KB starting from address 0x10000. The paged region can be located anywhere in the memory.

Accessing an address outside of the flat region will be automatically handled by the page fault mechanism, which allocates a new page and maps it to the faulting address. There can be up to 64KB or 16 pages of memory used for the paged region.

The runtime provides environment calls for memory allocation and management. These calls can usually be more memory efficient than implementing memory management in the program itself, as they store the metadata of allocated memory externally in the runtime instead of in the program's memory. Each of these calls has a instruction penalty of 2000, which can be larger than a memory management implementation in the program itself, if done efficiently. Therefore, players can choose to use the provided memory management calls or implement their own memory management scheme based on their needs.

Another downside of using the provided memory management calls is that these calls may overcommit the memory usage. Please do expect that sometimes `malloc` returned a valid pointer, but actually reading/writing to the pointer results in a out-of-memory crash.

## Runtime

There are two different runtimes in the game: base runtime and unit runtime. The base runtime is responsible for controlling the base, while the unit runtime is responsible for controlling individual units. The player can submit separate programs for the base and units.

The base runtime is started at the beginning of the game and runs until the game ends. The unit runtime is started whenever a new unit is manufactured and runs until the unit is destroyed.

Some environment calls are only available in the base runtime or unit runtime. The documentation of each environment call will specify its availability. Invoking a function that is exclusive to a different runtime will fail with error `UNSUPPORTED_RUNTIME`.

## Instruction Limit

The player's program does not need to explicitly yield control back to the game engine. Instead, the game engine will automatically pause the program after a certain number of instructions have been executed in each turn. The player's program can safely write dead loops or long computations, as the game will automatically separate the execution into multiple turns as if the program is running continuously.

The instruction limit per turn is set to 100K instructions for both base runtime and unit runtime. If the program exceeds the instruction limit in a turn, it will be forcibly paused, and the game will proceed to the next phase.

Some environment calls have additional instruction penalties, which will be added to the total instruction count when the call is invoked. The instruction penalty for each call is specified in the documentation of that call.

The penalty is applied regardless of whether the call succeeds or fails. Futhermore, the penalty will not be accumulated to the next turn, that is, if the program exceeds the instruction limit due to penalties, it will be paused immediately without carrying over the excess to the next turn.

## Environment Calls

The sandbox provides a set of environment calls that the player's program can use to interact with the game world. To invoke an environment call, the program should place the call number in register `a7`, the arguments in registers `a0` to `a5` and execute the `ecall` instruction. The return value will be placed in register `a0`.

### Aborting

**Call Number**: 0x00

**Signature**:

```c
noreturn void abort();
```

**Description**:

Aborts the execution of the program immediately. This can be used to signal an unrecoverable error. Please do not rely on this function as a method of restarting the program.

**Availability**: Base runtime and Unit runtime.

**Instruction Penalty**: not relevant

### Logging

**Call Number**: 0x01

**Signature**:

```c
int log(const char* str, int n);
```

**Description**:

Logs a message of `n` bytes from the provided `str` pointer to the game log. The function returns 0 on success, or negative error code on failure (e.g., invalid pointer).

The length `n` must be between 1 and 512, inclusive. The call will fail if the length is out of range with error `OUT_OF_RANGE`. The call will also fail if the pointer cannot be read with error `INVALID_POINTER`.

It is recommended to output ASCII strings only, as non-ASCII characters may not be displayed properly in the game log. The string can but not necessarily be null-terminated, as the length is explicitly provided.

**Availability**: Base runtime and Unit runtime.

**Instruction Penalty**: number of bytes logged.

### Gamerule & Meta Information

**Call Number**: 0x0F

**Signature**:

```c
struct GameInfo {
	uint8_t map_width;
	uint8_t map_height;
	uint8_t base_size;
	uint8_t reserved[13];
};

int meta(struct GameInfo* info);
```

**Description**:

Retrieves the gamerule and meta information of the current game and saves it to the provided `info` pointer. The `GameInfo` structure contains the following fields:

- `map_width`: the width of the map in tiles.
- `map_height`: the height of the map in tiles.
- `base_size`: the size of the base in tiles (the base is a square area of `base_size` x `base_size` tiles).
- `reserved`: reserved for future use and should be ignored.

The function returns 0 on success. It will fail if the pointer cannot be written to with error `INVALID_POINTER`.

**Availability**: Base runtime and Unit runtime.

**Instruction Penalty**: 0

### Turn Number

**Call Number**: 0x10

**Signature**:

```c
int turn();
```

**Description**:

Returns the current turn number. The turn number starts from 1 and increments by 1 at the beginning of each turn.

**Availability**: Base runtime and Unit runtime.

**Instruction Penalty**: 0

### Device Information

**Call Number**: 0x11

**Signature**:

```c
struct DeviceInfo {
	uint8_t id : 5;		// 0: base, positive integer: unit id
	uint8_t upgrades : 3; // bit 0: capacity upgrade, bit 1: vision upgrade, bit 2: damage upgrade, only valid for units
	uint8_t health;		// current health (only valid for units)
	uint16_t energy;	// current energy carried
} dev_info();
```

**Description**:

Returns information about the current device (base or unit). For the base, the upgrades and health fields are always zero.

**Availability**: Base runtime and Unit runtime.

**Instruction Penalty**: 0

### Sensor Reading

**Call Number**: 0x12

**Signature**:

```c
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
```

**Description**:

Reads the sensor data of surrounding tiles and saves it to the provided `data` array. Each element in the `data` array corresponds to a tile in the visible area of the device. The function returns 0 on success, or fail with `INVALID_POINTER` if the pointer cannot be written to.

The `data` array should have a size of 25 for base and units without vision upgrade, or 81 for units with vision upgrade. The tiles are ordered in row-major order, centered on the current device.

All fields in `SensorData` are only valid if `visible` is true. Otherwise, they should be ignored. The `alliance_unit` field is only valid if `has_unit` is true.

Returns the number of tiles successfully read on success, or a negative error code on failure.

**Availability**: Base runtime and Unit runtime.

**Instruction Penalty**: tiles read

### Message Receiving

**Call Number**: 0x13

**Signature**:

```c
int recv_msg(uint8_t buf[], int n);
```

**Description**:

Receives a message from the message queue of at most `n` bytes and saves it to the provided `buf` array. The function returns the number of bytes received on success, or 0 if there is no message in the queue, or negative error code on failure.

If the size of the message in the queue is greater than `n`, only the first `n` bytes are received, and the remaining bytes are discarded.

The length `n` must be non-negative. The `buf` pointer will not be dereferenced if `n` is zero, i.e. `receive_msg(nullptr, 0);` is a valid call to skip a message in the queue without writing to any buffer.

If the length `n` is negative, the call will fail with error `OUT_OF_RANGE`. If the pointer is cannot be written to, the call will fail with error `INVALID_POINTER`.

It is worth noting that messages sent in the same turn will be received in the next turn or later, that is, messages are not sent and received instantaneously. The device will receive messages sent by allied devices *including* itself. 

**Availability**: Base runtime and Unit runtime.

**Instruction Penalty**: number of bytes received.

### Message Sending

**Call Number**: 0x14

**Signature**:

```c
int send_msg(const uint8_t buf[], int n);
```

**Description**:

Sends a message of `n` bytes from the provided `buf` array to all allied devices. The length `n` must be between 1 and 512, inclusive. The function returns 0 on success, or negative error code on failure.

This function can only be invoked once per turn. Attempting to send more than one message in a turn will result in an error.

If the length `n` is out of range, the call will fail with error `OUT_OF_RANGE`. If the pointer cannot be read, the call will fail with error `INVALID_POINTER`. If a message has already been sent in the current turn, the call will fail with error `ON_COOLDOWN`.

It is worth noting that messages sent in the same turn will be received in the next turn or later, that is, messages are not sent and received instantaneously. The device will receive messages sent by allied devices *including* itself.

**Availability**: Base runtime and Unit runtime.

**Instruction Penalty**: 0

### Position Infomation

**Call Number**: 0x15

**Signature**:

```c
struct PosInfo {
	uint8_t x;
	uint8_t y;
	uint8_t reserved[2];
} pos();
```

**Description**:

Returns the position of current device. The `x` and `y` fields represent the coordinates of the device on the map. The top-left corner of the map is (0, 0). The `reserved` field is reserved for future use and should be ignored.

The position of the base is fixed throughout the game. The returned coordinates of the base are the coordinates of the top-left corner of the base.

**Availability**: Base runtime and Unit runtime.

**Instruction Penalty**: 0

### Manufacture Unit

**Call Number**: 0x20

**Signature**:

```c
int manufact(int id);
```

**Description**:

Manufactures a new unit with the specified `id`. The `id` must be a positive integer between 1 and 15, inclusive. The function returns 0 on success, or negative error code on failure.

The `id` must not have been previously assigned to an existing unit, or must belong to a unit that has been destroyed.

Manufacturing a new unit costs energy, if there isn't enough energy in the base, the call will fail with error `INSUFFICIENT_ENERGY`. If the `id` is out of range or already in use, the call will fail with error `INVALID_ID`.

The new unit spawns on a random empty tile within the base area. If all tiles in the base area are occupied, the call will fail with error `CAPACITY_FULL`.

This function can only be invoked once per turn (only successful manufacturing attempts count). Attempting to manufacture more than one unit in a turn will result in an `ON_COOLDOWN` error.

**Availability**: Base runtime only.

**Instruction Penalty**: 0

### Repair Unit

**Call Number**: 0x21

**Signature**:

```c
int repair(int id);
```

**Description**:

Repairs the unit with the specified `id`. The function returns 0 on success, or negative error code on failure (e.g., insufficient energy, invalid id, unit not in base area).

Repairing a unit costs energy, if there isn't enough energy in the base, the call will fail with error `INSUFFICIENT_ENERGY`. If the `id` is invalid, belongs to the base, The call will fail with error `INVALID_ID`. If the `id` belongs to a unit that is not currently in the base area or already at full health, the call will fail with error `INVALID_UNIT`.

This function can only be invoked once per turn (only successful repair attempts count). Attempting to repair more than one unit in a turn will result in an `ON_COOLDOWN` error.

**Availability**: Base runtime only.

**Instruction Penalty**: 0

### Upgrade Unit

**Call Number**: 0x22

**Signature**:

```c
int upgrade(int id, int type);
```

**Description**:

Upgrades the unit with the specified `id` by installing an upgrade of the specified `type`. The `type` can be one of the following:

- 0: Capacity Upgrade
- 1: Vision Upgrade
- 2: Damage Upgrade

The function returns 0 on success, or negative error code on failure. Upgrading a unit costs energy, if there isn't enough energy in the base, the call will fail with error `INSUFFICIENT_ENERGY`. If the `id` is invalid, belongs to the base, the call will fail with error `INVALID_ID`. If the `id` belongs to a unit that is not currently in the base area, or already has the specified upgrade, the call will fail with error `INVALID_UNIT`. If the `type` is not one of the above values, the call will fail with error `OUT_OF_RANGE`.

This function can only be invoked once per turn (only successful upgrade attempts count). Attempting to upgrade more than one unit in a turn will result in an `ON_COOLDOWN` error.

**Availability**: Base runtime only.

**Instruction Penalty**: 0

### Fire Bullet

**Call Number**: 0x30

**Signature**:

```c
int fire(int direction, int power);
```

**Description**:

Fires a bullet in the specified `direction`. The `direction` can be one of the following:

- 0: Up
- 1: Right
- 2: Down
- 3: Left

The function returns 0 on success, or negative error code on failure (e.g., insufficient energy, invalid direction).

The `power` parameter specifies the energy cost of firing the bullet. If the unit does not have enough energy, the call will fail with error `INSUFFICIENT_ENERGY`. If the `direction` is not one of the above values, the call will fail with error `OUT_OF_RANGE`.

The function can only be invoked once per turn. Furthermore, within the same turn and the next 2 turns after a successful fire, the unit cannot fire another bullet. Attempting to fire a bullet during the cooldown period will result in an `ON_COOLDOWN` error.

**Availability**: Unit runtime only.

**Instruction Penalty**: 0

### Move Unit

**Call Number**: 0x31

**Signature**:

```c
int move(int direction);
```

**Description**:

Moves the unit in the specified `direction`. The `direction` can be one of the following:

- 0: Up
- 1: Right
- 2: Down
- 3: Left

If the `direction` is not one of the above values, the call will fail with error `OUT_OF_RANGE`.

The function returns 0 on success, or negative error code on failure.

It is worth noting that a failed move attempt (due to obstacles, boundaries, etc.) is not considered as failing to invoke the function, i.e., the function will still return 0 in such cases.

If the function is invoked multiple times in the same turn, only the last invocation takes effect. If the last invocation has an invalid direction, there will be no movement in the next turn.

The move will take effect in the turn after the current turn, that is, the move taken in turn T will be executed at the beginning of turn T+1.

**Availability**: Unit runtime only.

**Instruction Penalty**: 0

### Deposit Energy

**Call Number**: 0x32

**Signature**:

```c
int deposit(int amount);
```

**Description**:

Deposits the specified `amount` of energy from the unit to the base. The function returns 0 on success, or negative error code on failure.

The `amount` must be positive. If the `amount` is not positive, the call will fail with error `OUT_OF_RANGE`. If the unit does not have enough energy, the call will fail with error `INSUFFICIENT_ENERGY`.

The function can only be invoked once per turn (only successful deposit attempts count). Futhermore, one unit can only invoke either `deposit` or `withdraw` in the same turn. Attempting to invoke `deposit` twice in the same turn, or invoking `deposit` after a successful invocation of `withdraw` in the same turn, will result in an `ON_COOLDOWN` error.

**Availability**: Unit runtime only.

**Instruction Penalty**: 0

### Withdraw Energy

**Call Number**: 0x33

**Signature**:
```c
int withdraw(int amount);
```

**Description**:

Withdraws the specified `amount` of energy from the base to the unit. The function returns 0 on success, or negative error code on failure (e.g., insufficient base energy, invalid amount).

The function can only be invoked once per turn (only successful withdraw attempts count). Futhermore, one unit can only invoke either `deposit` or `withdraw` in the same turn. Attempting to invoke `withdraw` twice in the same turn, or invoking `withdraw` after a successful invocation of `deposit` in the same turn, will result in an `ON_COOLDOWN` error.

**Availability**: Unit runtime only.

**Instruction Penalty**: 0

### Memory Allocation and Management

**Call Number**: 0x40-0x44

**Signature**:

```c
void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);
int meminfo(struct {
	size_t bytes_free;
	size_t bytes_used;
	size_t chunks_used;
} *info);
```

**Description**:

These functions provide native dynamic memory allocation and management within the sandbox. The `meminfo` function retrieves information about the current memory usage.

The `malloc`, `calloc`, `realloc`, and `free` functions behave similarly to their counterparts in the C standard library.

It's worth noting that these functions are implemented within the sandbox only for convenience. Players are free to implement their own memory management schemes if desired.

**Availability**: Base runtime and Unit runtime.

**Instruction Penalty**: 2000

### Random Number Generation

**Call Number**: 0x4F

**Signature**:

```c
uint32_t rand();
```

**Description**:

Returns a pseudo-random 32-bit unsigned integer. The random number generator is seeded at the start of each runtime instance based on internal game state, ensuring different sequences across different instances. The function uses Xoroshiro128++ algorithm for random number generation, which provides a good randomness quality.

**Availability**: Base runtime and Unit runtime.

**Instruction Penalty**: 0

## Error Handling and Error Codes

When an environment call fails, it will return a negative error code. The following error codes are defined:

- `INVALID_POINTER` (-1): The provided pointer cannot be dereferenced as required.
- `INVALID_UNIT` (-2): The specified unit does not meet the requirements for the action.
- `INSUFFICIENT_ENERGY` (-3): The device does not have enough energy to perform the action.
- `ON_COOLDOWN` (-4): The action cannot be performed this turn due to cooldown restrictions.
- `CAPACITY_FULL` (-5): The base has no empty tile to spawn a new unit.
- `INVALID_ID` (-6): The specified ID is invalid.
- `OUT_OF_RANGE` (-7): The specified value is out of the valid range.
- `UNSUPPORTED_RUNTIME` (-8): The function is not supported in the current runtime.

