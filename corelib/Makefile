CC = riscv64-elf-gcc
AR = riscv32-elf-ar
CFLAGS = -O2 -ffreestanding -nostdlib -nostdinc -march=rv32g -mabi=ilp32 -Wall -ffunction-sections -fdata-sections
TEST_CFLAGS = -I. -L. -lcorelib -static -Wl,--gc-sections

all: libcorelib.a test_base.elf test_dummy.elf test_heap.elf test_misc.elf test_msg.elf

corelib.o: corelib.c
	$(CC) $(CFLAGS) -c corelib.c -o corelib.o

libcorelib.a: corelib.o
	$(AR) rcs libcorelib.a corelib.o

test_base.elf: test/base.c libcorelib.a
	$(CC) $(CFLAGS) $(TEST_CFLAGS) test/base.c -o test_base.elf

test_dummy.elf: test/dummy.c libcorelib.a
	$(CC) $(CFLAGS) $(TEST_CFLAGS) test/dummy.c -o test_dummy.elf

test_heap.elf: test/heap.c libcorelib.a
	$(CC) $(CFLAGS) $(TEST_CFLAGS) test/heap.c -o test_heap.elf

test_misc.elf: test/misc.c libcorelib.a
	$(CC) $(CFLAGS) $(TEST_CFLAGS) test/misc.c -o test_misc.elf

test_msg.elf: test/msg.c libcorelib.a
	$(CC) $(CFLAGS) $(TEST_CFLAGS) test/msg.c -o test_msg.elf

.PHONY: all clean symlinks

clean:
	rm -f *.o *.a *.elf
