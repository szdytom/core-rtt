#include "corertt/xoroshiro.h"
#include <bit>
#include <charconv>
#include <format>
#include <initializer_list>
#include <random>

namespace cr {

namespace {

bool is_raw_seed_format(std::string_view text) noexcept {
	if (text.size() != 32) {
		return false;
	}

	for (char c : text) {
		const bool digit = c >= '0' && c <= '9';
		const bool lower = c >= 'a' && c <= 'f';
		const bool upper = c >= 'A' && c <= 'F';
		if (!(digit || lower || upper)) {
			return false;
		}
	}

	return true;
}

std::uint64_t parse_hex_u64_16(std::string_view text) noexcept {
	std::uint64_t value = 0;
	const auto *begin = text.data();
	const auto *end = begin + text.size();
	const auto parse_result = std::from_chars(begin, end, value, 16);
	if (parse_result.ec != std::errc{} || parse_result.ptr != end) {
		return 0;
	}
	return value;
}

} // namespace

Seed Seed::from_string(std::string_view str) noexcept {
	if (is_raw_seed_format(str)) {
		return Seed{
			.s = {
				parse_hex_u64_16(str.substr(0, 16)),
				parse_hex_u64_16(str.substr(16, 16)),
			}
		};
	}

	Seed res{0xfcc3a80ff25bae88, 0x78ac504431a5b8e6};
	constexpr std::uint64_t p1 = 0xb2209ed48ff3455b, p2 = 0x9f9a70d28f55f29f;
	for (size_t i = 0; i < str.size(); ++i) {
		std::uint8_t c = str[i];
		res.s[0] ^= c;
		res.s[1] ^= c;
		res.s[0] *= p1;
		res.s[1] *= p2;
	}
	return res;
}

std::string Seed::to_string() const {
	return std::format("{:016x}{:016x}", s[0], s[1]);
}

Seed Seed::device_random() {
	Seed seed;
	std::random_device rd;
	std::uniform_int_distribution<std::uint64_t> dist(
		std::numeric_limits<std::uint64_t>::min(),
		std::numeric_limits<std::uint64_t>::max()
	);
	seed.s[0] = dist(rd);
	seed.s[1] = dist(rd);
	return seed;
}

Xoroshiro128PP::Xoroshiro128PP(Seed seed) noexcept: seed(seed) {}

std::uint64_t Xoroshiro128PP::next() noexcept {
	std::uint64_t s0 = seed.s[0];
	std::uint64_t s1 = seed.s[1];
	std::uint64_t result = std::rotl(s0 + s1, 17) + s0;

	s1 ^= s0;
	seed.s[0] = std::rotl(s0, 49) ^ s1 ^ (s1 << 21);
	seed.s[1] = std::rotl(s1, 28);

	return result;
}

Xoroshiro128PP Xoroshiro128PP::jump_64() const noexcept {
	constexpr std::uint64_t JUMP_64[] = {
		0x180ec6d33cfd0aba, 0xd5a61266f0c9392c
	};

	Xoroshiro128PP res{seed};

	for (int i : {0, 1}) {
		for (int b = 0; b < 64; ++b) {
			if (JUMP_64[i] & (1ULL << b)) {
				res.seed.s[0] ^= seed.s[0];
				res.seed.s[1] ^= seed.s[1];
			}
			res.next();
		}
	}

	return res;
}

Xoroshiro128PP Xoroshiro128PP::jump_96() const noexcept {
	constexpr std::uint64_t JUMP_96[] = {
		0x360fd5f2cf8d5d99, 0x9c6e6877736c46e3
	};

	Xoroshiro128PP res{seed};

	for (int i : {0, 1}) {
		for (int b = 0; b < 64; ++b) {
			if (JUMP_96[i] & (1ULL << b)) {
				res.seed.s[0] ^= seed.s[0];
				res.seed.s[1] ^= seed.s[1];
			}
			res.next();
		}
	}

	return res;
}

Xoroshiro128PP &Xoroshiro128PP::globalInstance() noexcept {
	static Xoroshiro128PP instance(Seed::device_random());
	return instance;
}

} // namespace cr