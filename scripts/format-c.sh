#!/bin/bash
set -e
cd "$(git rev-parse --show-toplevel)"
find . -type d -name build -prune -o -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print0 | xargs -0 clang-format -i
