#ifndef CORERTT_FAIL_FAST_H
#define CORERTT_FAIL_FAST_H

#include <cpptrace/basic.hpp>
#include <cstdlib>
#include <iostream>
#include <utility>

#define CR_FAIL_FAST_ABORT(err_message)              \
	do {                                             \
		std::cerr << (err_message) << '\n';          \
		cpptrace::generate_trace().print(std::cerr); \
		std::abort();                                \
	} while (false)

#ifndef NDEBUG

// Lightweight assertion, should be used for checking simple conditions like a <
// b with no side effects and no expensive computations.
#define CR_FAIL_FAST_ASSERT_LIGHT(cond, err_message) \
	do {                                             \
		if (!(cond)) {                               \
			CR_FAIL_FAST_ABORT(err_message);         \
		}                                            \
	} while (false)

// Heavyweight assertion, should be used for checking complex conditions that
// may involve side effects or expensive computations. The condition will only
// be evaluated in debug builds.
#define CR_FAIL_FAST_ASSERT_HEAVY(cond, err_message) \
	do {                                             \
		if (!(cond)) {                               \
			CR_FAIL_FAST_ABORT(err_message);         \
		}                                            \
	} while (false)

#else

#define CR_FAIL_FAST_ASSERT_LIGHT(cond, err_message) \
	do {                                             \
		if (!(cond)) {                               \
			std::unreachable();                      \
		}                                            \
	} while (false)

#define CR_FAIL_FAST_ASSERT_HEAVY(cond, err_message) ((void)0)

#endif // NDEBUG

#endif // CORERTT_FAIL_FAST_H
