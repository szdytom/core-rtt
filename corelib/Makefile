# Makefile for building the core library and test executables

# We are working in the RISCV 32-bit environment, however, most distros like Arch Linux only provide a single riscv64-elf-gcc which can target both 32-bit and 64-bit. So we use riscv64-elf-gcc with -mabi=ilp32 to generate 32-bit code.
CC = riscv64-elf-gcc
# The binary utilities (ar, objcopy, etc.) are separate and we need the 32-bit version of ar to create the library archive.
AR = riscv32-elf-ar

# Compiler flags for both library and tests
CFLAGS = -O2 -ffreestanding -nostdlib -nostdinc \
         -march=rv32g -mabi=ilp32 \
         -Wall -ffunction-sections -fdata-sections \
         -I. # include current dir for corelib.h

# Linker flags (used only when linking executables)
LDFLAGS = -static -Wl,--gc-sections

# Libraries to link against for tests
TEST_LIBS = -L. -lcorelib

# Source files
LIB_SRC = corelib.c
LIB_OBJ = $(LIB_SRC:.c=.o)

# List of test source files and corresponding ELF targets
TEST_SRCS = test/base.c test/dummy.c test/heap.c \
            test/misc.c test/msg.c
TEST_ELFS = $(TEST_SRCS:.c=.elf)

# Default target: build library and all test executables
all: libcorelib.a $(TEST_ELFS)

# Rule to compile the library object file and generate dependency files (*.d)
$(LIB_OBJ): %.o: %.c corelib.h
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Archive the library
libcorelib.a: $(LIB_OBJ)
	$(AR) rcs $@ $^

# Pattern rule to build each test executable
# Depends on the library, the source file, and the public header (to recompile
# when the API changes). Dependency files (*.d) are also generated.
$(TEST_ELFS): %.elf: %.c libcorelib.a corelib.h
	$(CC) $(CFLAGS) $(LDFLAGS) -MMD -MP $< $(TEST_LIBS) -o $@

# Include automatically generated dependency files (if they exist)
-include $(LIB_OBJ:.o=.d) $(TEST_ELFS:.elf=.d)

# Phony targets (not real files)
.PHONY: all clean

# Clean up all generated files
clean:
	rm -f *.o *.a *.elf *.d
