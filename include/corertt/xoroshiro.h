#ifndef CORERTT_XOROSHIRO_H
#define CORERTT_XOROSHIRO_H

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace cr {

struct Seed {
	std::uint64_t s[2];

	static Seed fromString(std::string_view str) noexcept;
	std::string toString() const;
	static Seed deviceRandom();
};

/**
 * @brief Xoroshiro128++ random number generator.
 * @note This generator is not thread-safe and should not be used concurrently.
 * It is designed for high performance and provides a good quality of
 * randomness.
 * @link https://prng.di.unimi.it/xoroshiro128plusplus.c @endlink
 */
class Xoroshiro128PP {
public:
	Xoroshiro128PP() noexcept = default;
	explicit Xoroshiro128PP(Seed seed) noexcept;

	// Adaption for STL RandomEngine named requirements
	using result_type = std::uint64_t;
	static constexpr result_type min() noexcept {
		return std::numeric_limits<result_type>::min();
	}
	static constexpr result_type max() noexcept {
		return std::numeric_limits<result_type>::max();
	}
	// Equivalent to next(), for STL compatibility
	result_type operator()() noexcept {
		return next();
	}

	/**
	 * @brief Generates a random 64-bit unsigned integer.
	 * @return A random 64-bit unsigned integer.
	 * @note This function will modify the internal state of the generator.
	 * It is not thread-safe and should not be called concurrently.
	 * The generated number is uniformly distributed.
	 */
	std::uint64_t next() noexcept;

	/**
	 * @brief Equivalent to 2^64 calls to next(), returns a new generator state.
	 * @return A new Xoroshiro128PP generator state.
	 * @note It can be used to generate two 2^64 non-overlapping sequences of
	 * random numbers for parallel processing.
	 */
	Xoroshiro128PP jump64() const noexcept;

	/**
	 * @brief Equivalent to 2^96 calls to next(), returns a new generator state.
	 * @return A new Xoroshiro128PP generator state.
	 * @note It can be used to generate 2^32 starting points, from each of which
	 * `jump64()` will generate 2^32 non-overlapping subsequences for parallel
	 * distributed computations.
	 */
	Xoroshiro128PP jump96() const noexcept;

	static Xoroshiro128PP &globalInstance() noexcept;

private:
	Seed seed;
};

} // namespace cr

#endif
