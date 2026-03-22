# Core RTT

A real-time tile-based game where players program their units' behavior rather than controlling them directly. The game is designed to be simple and accessible, with a focus on programming and strategy.

## Building

The game itself is a C++ application using CMake as its build system. The standard CMake build process applies, and all third-party dependencies are handled automatically. Please ensure that you have CMake and a C++23 compatible compiler installed (e.g. GCC 14, Clang 20, MSVC 19.44.35219.0) installed.

```sh
# Make sure to enter MSVC environment when on Windows
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo .
cmake --build build --config RelWithDebInfo
```

The resulting executable will be located at `build/core-rtt` (or `build\core-rtt.exe` on Windows).

The corelib, the support library to provide basic utilities and functions for user programs, is a C library using traditional Makefile as its build system. To build the corelib, you need to have a RISC-V 32-bit cross-compilation toolchain installed.

```sh
cd corelib
CC=<path-to-riscv32-compiler> AR=<path-to-riscv32-archiver> make libcorelib.a
```

For example, on Arch Linux, you can install the `riscv32-elf-binutils` and `riscv64-elf-gcc` packages, and then use `riscv64-elf-gcc` as the compiler and `riscv64-elf-ar` as the archiver. For reasons why the 64-bit compiler can be used to build 32-bit binaries, please refer to the comments in the Makefile.

## Game Rules and Programming

Please refer to

- [Core RTT Rules](docs/rules.md) for information about the game rules.
- [API Specification](docs/api-spec.md) for details on the programming environment and available machine level APIs.
- [Corelib Documentation](docs/corelib.md) for the C library that provides a wrapper around the machine level APIs, making it easier to write user programs, and more utility functions.
- [Corelib Example Programs](corelib/example) for some example programs written using the corelib.
- [Corelib Test Programs](corelib/test) for some example programs written using the corelib.

## License

The game itself and the corelib are both open sourced under the MIT License. See the LICENSE file for details.
