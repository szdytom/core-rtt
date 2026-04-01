# Makefile for building the corelib and test executables
# Supports both GNU and LLVM toolchains. Use CLANG=1 to select LLVM.

# ISA specification (can be overridden from command line)
MARCH = rv32g_zba_zbb_zbc_zbs

# Toolchain selection: if CLANG=1, use clang and llvm-ar; otherwise use GNU toolchain for RISC-V
# We are working in the RISCV 32-bit environment, however, most distros like Arch Linux only provide a single riscv64-elf-gcc which can target both 32-bit and 64-bit. So we use riscv64-elf-gcc with -mabi=ilp32 to generate 32-bit code.
ifdef CLANG
    CC = clang
    AR = llvm-ar
    # Add target triple for clang cross-compilation
    CFLAGS += -target riscv32-none-elf
    LDFLAGS += -fuse-ld=lld -Wl,--no-rosegment
else
    CC = riscv64-elf-gcc
    AR = riscv32-elf-ar
endif

# Compiler flags for both library and tests
CFLAGS += -O2 -ffreestanding -nostdlib -nostdinc \
          -march=$(MARCH) -mabi=ilp32d \
          -Wall -ffunction-sections -fdata-sections \
          -I. # include current dir for corelib.h

# Linker flags (used only when linking executables)
LDFLAGS += -static -Wl,--gc-sections -T link.ld

# Libraries to link against for tests
TEST_LIBS = -L. -lcorelib

# Source files
LIB_SRC = corelib.c
LIB_OBJ = $(LIB_SRC:.c=.o)

AUTOTEST_SRCS = $(wildcard autotest/*.c)

# List of test source files and corresponding ELF targets
EXAMPLE_SRCS = $(wildcard test/*.c example/*.c)
ALL_TEST_SRCS = $(EXAMPLE_SRCS) $(AUTOTEST_SRCS)
TEST_OBJS = $(ALL_TEST_SRCS:.c=.o)
TEST_ELFS = $(ALL_TEST_SRCS:.c=.elf)

# Default target: build library and all test executables
all: libcorelib.a $(TEST_ELFS)

# Rule to compile the library object file and generate dependency files (*.d)
$(LIB_OBJ): %.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Archive the library
libcorelib.a: $(LIB_OBJ)
	$(AR) rcs $@ $^

# Pattern rule to build each test executable
$(TEST_OBJS): %.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(TEST_ELFS): %.elf: %.o libcorelib.a link.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(TEST_LIBS) -o $@

# Include automatically generated dependency files (if they exist)
-include $(LIB_OBJ:.o=.d) $(TEST_ELFS:.elf=.d)

.PHONY: all clean

# Clean up all generated files
clean:
	rm -f *.o *.a *.elf *.d test/*.o test/*.elf test/*.d autotest/*.o autotest/*.elf autotest/*.d example/*.o example/*.elf example/*.d
