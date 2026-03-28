General Instructions:
- Use English for all comments in code.
- Write comments using Markdown syntax, even though they are not rendered. For example, "// `object` is borrowed", not "// object is borrowed".
- Keep comments concise: let the code explain itself if possible.

C++ Specific Instructions:
- Use the latest C++23 features whenever applicable.
- Use snake_case for variables, camelCase for function and method names, PascalCase for class names, and SCREAMING_SNAKE_CASE for constants, enum values, and macros. Prefix private fields and methods with an underscore.
- Fail-fast for error handling: prefer `CR_FAIL_FAST_ASSERT_*` macros over exceptions or error codes for internal logic errors. See `include/corertt/fail_fast.h` for details.
- Mark functions with `noexcept` if they do not throw exceptions (or only `std::bad_alloc`).
- Don't use `static_cast` when an implicit conversion is automatically applied.
- Use `std::to_underlying` for enum to integer conversions instead of `static_cast`.
- Use `scripts/format-c.sh` to format all C/C++ source files (it detects which files automatically, no need to specify file paths).
- Use `scripts/autotest.sh` to run all tests.

Rust Specific Instructions:
- Use the latest Rust features whenever applicable.
- Use the usual Rust naming conventions (see Rust RFC 430).
- Any `unsafe` block must be preceded by a `// SAFETY:` comment describing why the code inside is sound. Even if trivial, this comment is required to ensure that no extra implicit constraints are applied to the reasoning.

JavaScript/TypeScript Specific Instructions:
- Use `pnpm` as the package manager. Place all JavaScript/TypeScript-related code in the `js` directory (pnpm monorepo).
- Use the latest ECMAScript features whenever applicable.
- Use `camelCase` for variable and function names, `PascalCase` for class names and types, and `SCREAMING_SNAKE_CASE` for constants. Never use `#private` fields in TypeScript. Never include `$` in identifiers. Never use `_` as a prefix, except to indicate unused variables.
