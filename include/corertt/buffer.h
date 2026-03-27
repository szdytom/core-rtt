#ifndef CORERTT_BUFFER_H
#define CORERTT_BUFFER_H

#include "corertt/stream_adapter.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace cr {

class WriteBuffer {
public:
	template<typename... Args>
	void write(Args &&...args) noexcept {
		(writeOne(std::forward<Args>(args)), ...);
	}

	void writeU8(std::uint8_t value) noexcept;
	void writeI8(std::int8_t value) noexcept;
	void writeU16(std::uint16_t value) noexcept;
	void writeI16(std::int16_t value) noexcept;
	void writeU32(std::uint32_t value) noexcept;
	void writeI32(std::int32_t value) noexcept;
	void writeBytes(std::span<const std::byte> bytes) noexcept;

	void patchU32(std::size_t offset, std::uint32_t value) noexcept;
	std::size_t size() const noexcept;
	const std::vector<std::byte> &bytes() const noexcept;
	std::vector<std::byte> take() noexcept;

private:
	template<typename>
	static constexpr bool always_false = false;

	template<typename T>
	void writeOne(T &&value) noexcept {
		using value_type = std::remove_cvref_t<T>;
		if constexpr (std::is_enum_v<value_type>) {
			writeOne(std::to_underlying(value));
		} else if constexpr (
			std::same_as<value_type, std::span<const std::byte>>
		) {
			writeBytes(value);
		} else if constexpr (std::same_as<value_type, std::span<std::byte>>) {
			writeBytes(std::span<const std::byte>(value));
		} else if constexpr (std::integral<value_type>) {
			if constexpr (sizeof(value_type) == 1) {
				if constexpr (std::is_signed_v<value_type>) {
					writeI8(value);
				} else {
					writeU8(value);
				}
			} else if constexpr (sizeof(value_type) == 2) {
				if constexpr (std::is_signed_v<value_type>) {
					writeI16(value);
				} else {
					writeU16(value);
				}
			} else if constexpr (sizeof(value_type) == 4) {
				if constexpr (std::is_signed_v<value_type>) {
					writeI32(value);
				} else {
					writeU32(value);
				}
			} else {
				static_assert(
					always_false<value_type>, "Unsupported integer size"
				);
			}
		} else {
			static_assert(
				always_false<value_type>, "Unsupported WriteBuffer type"
			);
		}
	}

	std::vector<std::byte> _buffer;
};

class ReadBuffer {
public:
	explicit ReadBuffer(StreamAdapter &stream) noexcept;

	bool has(std::size_t size);
	void require(std::size_t size);

	std::uint8_t readU8();
	std::int8_t readI8();
	std::uint16_t readU16();
	std::int16_t readI16();
	std::uint32_t readU32();
	std::int32_t readI32();

	std::span<const std::byte> peek(std::size_t size);
	std::span<const std::byte> peek(std::size_t left, std::size_t right);
	std::span<const std::byte> readBytes(std::size_t size);
	void take(std::size_t size, std::span<std::byte> out);
	void skip(std::size_t size);

	std::size_t position() const noexcept;
	bool end();

private:
	StreamAdapter &_stream;
	std::vector<std::byte> _scratch;
};

} // namespace cr

#endif
