#include "corertt/buffer.h"
#include "corertt/fail_fast.h"
#include <cstring>

namespace cr {

void WriteBuffer::writeU8(std::uint8_t value) noexcept {
	_buffer.push_back(static_cast<std::byte>(value));
}

void WriteBuffer::writeI8(std::int8_t value) noexcept {
	writeU8(static_cast<std::uint8_t>(value));
}

void WriteBuffer::writeU16(std::uint16_t value) noexcept {
	_buffer.push_back(static_cast<std::byte>(value & 0xFF));
	_buffer.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
}

void WriteBuffer::writeI16(std::int16_t value) noexcept {
	writeU16(static_cast<std::uint16_t>(value));
}

void WriteBuffer::writeU32(std::uint32_t value) noexcept {
	_buffer.push_back(static_cast<std::byte>(value & 0xFF));
	_buffer.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
	_buffer.push_back(static_cast<std::byte>((value >> 16) & 0xFF));
	_buffer.push_back(static_cast<std::byte>((value >> 24) & 0xFF));
}

void WriteBuffer::writeI32(std::int32_t value) noexcept {
	writeU32(static_cast<std::uint32_t>(value));
}

void WriteBuffer::writeBytes(std::span<const std::byte> bytes) noexcept {
	_buffer.insert(_buffer.end(), bytes.begin(), bytes.end());
}

void WriteBuffer::patchU32(std::size_t offset, std::uint32_t value) noexcept {
	CR_FAIL_FAST_ASSERT_LIGHT(
		offset + sizeof(std::uint32_t) <= _buffer.size(),
		std::format(
			"offset {} out of bounds(buffer size {}) U32", offset,
			_buffer.size()
		)
	);
	_buffer[offset] = static_cast<std::byte>(value & 0xFF);
	_buffer[offset + 1] = static_cast<std::byte>((value >> 8) & 0xFF);
	_buffer[offset + 2] = static_cast<std::byte>((value >> 16) & 0xFF);
	_buffer[offset + 3] = static_cast<std::byte>((value >> 24) & 0xFF);
}

std::size_t WriteBuffer::size() const noexcept {
	return _buffer.size();
}

const std::vector<std::byte> &WriteBuffer::bytes() const noexcept {
	return _buffer;
}

std::vector<std::byte> WriteBuffer::take() noexcept {
	return std::move(_buffer);
}

ReadBuffer::ReadBuffer(StreamAdapter &stream) noexcept: _stream(stream) {}

bool ReadBuffer::has(std::size_t size) {
	return _stream.has(size);
}

void ReadBuffer::require(std::size_t size) {
	CR_FAIL_FAST_ASSERT_HEAVY(has(size), "requested bytes are unavailable");
}

std::uint8_t ReadBuffer::readU8() {
	require(1);
	const auto bytes = _stream.peek(1);
	_stream.skip(1);
	return std::to_integer<std::uint8_t>(bytes[0]);
}

std::int8_t ReadBuffer::readI8() {
	return static_cast<std::int8_t>(readU8());
}

std::uint16_t ReadBuffer::readU16() {
	require(2);
	const auto bytes = _stream.peek(2);
	_stream.skip(2);
	const auto b0 = std::to_integer<std::uint16_t>(bytes[0]);
	const auto b1 = std::to_integer<std::uint16_t>(bytes[1]);
	return static_cast<std::uint16_t>(b0 | (b1 << 8));
}

std::int16_t ReadBuffer::readI16() {
	return static_cast<std::int16_t>(readU16());
}

std::uint32_t ReadBuffer::readU32() {
	require(4);
	const auto bytes = _stream.peek(4);
	_stream.skip(4);
	const auto b0 = std::to_integer<std::uint32_t>(bytes[0]);
	const auto b1 = std::to_integer<std::uint32_t>(bytes[1]);
	const auto b2 = std::to_integer<std::uint32_t>(bytes[2]);
	const auto b3 = std::to_integer<std::uint32_t>(bytes[3]);
	return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

std::int32_t ReadBuffer::readI32() {
	return static_cast<std::int32_t>(readU32());
}

std::span<const std::byte> ReadBuffer::peek(std::size_t size) {
	require(size);
	return _stream.peek(size);
}

std::span<const std::byte> ReadBuffer::peek(
	std::size_t left, std::size_t right
) {
	CR_FAIL_FAST_ASSERT_LIGHT(
		left <= right, std::format("invalid range ({}, {})", left, right)
	);
	require(right);
	return _stream.peek(left, right);
}

std::span<const std::byte> ReadBuffer::readBytes(std::size_t size) {
	require(size);
	_scratch.resize(size);
	_stream.take(size, _scratch);
	return std::span<const std::byte>(_scratch);
}

void ReadBuffer::take(std::size_t size, std::span<std::byte> out) {
	require(size);
	CR_FAIL_FAST_ASSERT_LIGHT(
		out.size() >= size,
		std::format("output span size {} < requested size {}", out.size(), size)
	);
	_stream.take(size, out);
}

void ReadBuffer::skip(std::size_t size) {
	require(size);
	_stream.skip(size);
}

std::size_t ReadBuffer::position() const noexcept {
	return _stream.position();
}

bool ReadBuffer::end() {
	return _stream.end();
}

} // namespace cr
