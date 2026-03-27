#!/bin/sh
set -e
cd "$(git rev-parse --show-toplevel)"

# Check if clang-format is of version 22 or higher
if ! clang-format --version | grep -q "version 2[2-9]"; then
	echo "Error: clang-format version 22 or higher is required."
	echo "Please install clang-format 22 or higher and try again."
	exit 1
fi

find . -type d -name build -prune -o -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print0 | xargs -0 clang-format -i
