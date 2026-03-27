#!/bin/sh
set -e
cd "$(git rev-parse --show-toplevel)"

# Check if ./build/corertt_headless exists, if not, print an error message
if [ ! -f "./build/corertt_headless" ]; then
	echo "Error: ./build/corertt_headless does not exist."
	echo "Please invoke cmake to build the project before running autotest.sh."
	echo "Refer to README.md for more information."
	exit 1
fi

# Check if corelib is built, we'll assume that if corelib/libcorelib.a exists, then corelib is built along with all the tests.
# This is not solid, maybe someone will invoke `make libcorelib.a` directly, but it's good enough for now.
if [ ! -f "./corelib/libcorelib.a" ]; then
	echo "Error: corelib/libcorelib.a does not exist."
	echo "Please build corelib and all the tests using corelib/Makefile before running autotest.sh."
	echo "Refer to README.md for more information."
	exit 1
fi

cd ./js

# Build the project, run typecheck and run tests
pnpm run build
pnpm run typecheck
pnpm run test

# Run the autotests
pnpm --filter='@corertt/core-autotest' exec node dist/main.js
