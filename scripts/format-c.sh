#!/bin/sh
set -e
cd "$(git rev-parse --show-toplevel)"

# Check if clang-format is of version 22 or higher
if ! clang-format --version | grep -q "version 2[2-9]"; then
	echo "Error: clang-format version 22 or higher is required."
	echo "Please install clang-format 22 or higher and try again."
	exit 1
fi

clang-format -i src/**/*.cpp include/**/*.h corelib/*.c corelib/*.h corelib/**/*.c src/*.cpp
