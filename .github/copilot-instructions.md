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
- Use `scripts/format-c.sh` to format all C/C++ source files.

JavaScript/TypeScript Specific Instructions:
- Use the latest ECMAScript features whenever applicable.
