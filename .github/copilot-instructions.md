General Instructions:
- Use English for all comments in code.
- Keep comments concise: let the code explain itself if possible.
- Use snake_case for variable, camelCase for function and method names, PascalCase for class names, and SCREAMING_SNAKE_CASE for constants, enum values and macros.

C++ Specific Instructions:
- Use the latest C++23 features whenever applicable.
- Fail-fast for error handling: prefer `CR_FAIL_FAST_ASSERT_*` macros over exceptions or error codes for internal logic errors. See `include/corertt/fail_fast.h` for details.
- Mark functions that do not throw exceptions, or only `std::bad_alloc`, with `noexcept`.
- Don't perform `static_cast` if a implicit conversion is automatically applied.
- Use `std::to_underlying` for enum to integer conversions instead of `static_cast`.
- Use `scripts/format-c.sh` to format all C/C++ source files (it detects which files automatically, no need to specify file paths).
- Use `scripts/autotest.sh` to run all tests.

JavaScript/TypeScript Specific Instructions:
- Use `pnpm` as the package manager, all JavaScript/TypeScript related things should be placed in the `js` directory (monorepo style).
- Use the latest ECMAScript features whenever applicable.
