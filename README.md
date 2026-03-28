# Core RTT

A real-time tile-based game where players program their units' behavior rather than controlling them directly. The game is designed to be simple and accessible, with a focus on programming and strategy.

## Building

The game itself is a C++ application using CMake as its build system. The standard CMake build process applies, and all third-party dependencies are handled automatically. Please ensure that you have CMake and a C++23 compatible compiler installed (e.g. GCC 14, Clang 20, MSVC 19.44.35219.0) installed.

```sh
# Make sure to enter MSVC environment when on Windows
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo .
cmake --build build --config RelWithDebInfo
```

The resulting executable will be located at `build/corertt` (or `build\corertt.exe` on Windows).

### Building Corelib (optional)

The corelib, the support library to provide basic utilities and functions for user programs, is a C library using traditional Makefile as its build system. To build the corelib, you need to have a RISC-V 32-bit cross-compilation toolchain installed.

```sh
cd corelib
make CC=<path-to-riscv32-compiler> AR=<path-to-riscv32-archiver>
```

For example, on Arch Linux, you can install the `riscv32-elf-binutils` and `riscv64-elf-gcc` packages, and then use `riscv64-elf-gcc` as the compiler and `riscv64-elf-ar` as the archiver. For reasons why the 64-bit compiler can be used to build 32-bit binaries, please refer to the comments in the Makefile.

Alternatively, you can also use LLVM toolchain to build the corelib, which can be easier to set up on some platforms, since `clang` cross-compilation support is often included in the default installation out-of-the-box. To use LLVM toolchain, you need to install `clang`, `llvm-ar`, and `lld` (the LLVM linker). Then you can build the corelib with the following command:

```sh
cd corelib
make CLANG=1
```

Specify `-j<num-threads>` to speed up the build with parallel compilation, for example, `make CLANG=1 -j$(nproc)` on Linux will use all available CPU cores.

### Building Rusty-corelib (optional)

The `rusty-corelib` is a Rust support library for writing guest programs in Rust, which can serve as an alternative to the C-based `corelib`. It uses Cargo as its build system. To build `rusty-corelib`, you need to have Rust and the RISC-V 32-bit target installed.

```sh
rustup target add riscv32imac-unknown-none-elf
cargo build --release
cargo build --release --examples
```

## Runtime Tools

This repository now builds two runtime executables:

- `corertt`: runtime mode with selectable UI (`--ui-mode=tui|plain`, default `tui`). It supports live simulation and replay playback.
- `corertt_headless`: a non-interactive mode without UI. It runs full-speed simulation and writes a binary replay file for later analysis or playback.

## Game Rules and Programming

Please refer to

- [Core RTT Rules](docs/rules.md) for information about the game rules.
- [API Specification](docs/api-spec.md) for details on the programming environment and available machine level APIs.
- [Corelib Documentation](docs/corelib.md) for the C library that provides a wrapper around the machine level APIs, making it easier to write user programs, and more utility functions.
- [Corelib Example Programs](corelib/example) for some example programs written using the corelib.
- [Rusty-corelib Documentation](docs/rusty-corelib.md) for the Rust support library, which provides similar functionalities as the corelib but with a Rust interface.
- [Rusty-corelib Example Programs](rusty-corelib/examples) for some example programs written using the rusty-corelib.

## Project Structure / Contributing

The project is structured as follows:

- `include/`: header files for the game engine and runtime.
- `src/`: the source code of the game engine and runtime.
- `CMakeLists.txt`: the CMake build configuration for the game engine and runtime.
- `corelib/`: the C library for user programs, with its own Makefile.
- `corelib/autotest/`: a set of test cases for the corelib, which can be tested automatically. Refer to `docs/autotesting.md` for details.
- `rusty-corelib/`: the Rust support library for user programs, with its own Cargo.toml and build configuration.
- `rusty-corelib/autotest/`: a set of test cases for the rusty-corelib, which can be tested automatically. Refer to `docs/autotesting.md` for details.
- `js/`: where all the JavaScript/TypeScript related code lives.
- `docs/`: documentation files for the game rules and development.

The `.clang-format` and `.rustfmt.toml` configures the code formatting. You may also want to read `.github/copilot-instructions.md` for conventions and guidelines to follow when contributing to the codebase (Even though the instructions are originally written for AI Assistants, they are also applicable to human contributors). Not 100% of our codebase strictly follows the instructions in that file, especially the older code, but we are trying to get closer to those guidelines over time (PR welcome if you see some code that can be easily improved to follow those guidelines better).

## License

The game itself and the corelib are both open sourced under the MIT License. See the LICENSE file for details.
