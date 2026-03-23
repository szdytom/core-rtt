General Instructions:
- Use English for all comments in code.
- Keep comments concise: let the code explain itself if possible.
- Use snake_case for variable, camelCase for function and method names, and PascalCase for class names.

C++ Specific Instructions:
- Use the latest C++23 features whenever applicable.
- Fail-fast for error handling: prefer throwing exceptions for errors related to external inputs, aborting for internal logic errors (log the error message to stderr and invoke `cpptrace::generate_trace().print(std::cerr)` before aborting).
- Mark functions that do not throw exceptions with `noexcept`.
- Don't perform `static_cast` if a implicit conversion is automatically applied.
- Use `std::to_underlying` for enum to integer conversions instead of `static_cast`.
- Use `find . -type d -name build -prune -o -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print0 | xargs -0 clang-format -i` to format all source files (Linux only).

JavaScript/TypeScript Specific Instructions:
- Use the latest ECMAScript features whenever applicable.
