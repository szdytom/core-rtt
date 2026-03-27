#include "corertt/stream_adapter.h"
#include "corertt/fail_fast.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <istream>

namespace cr {

MemoryStreamAdapter::MemoryStreamAdapter(std::span<const std::byte> bytes
) noexcept
	: _bytes(bytes) {}

bool MemoryStreamAdapter::has(std::size_t size) {
	return _cursor + size <= _bytes.size();
}

std::span<const std::byte> MemoryStreamAdapter::peek(std::size_t size) {
	CR_FAIL_FAST_ASSERT_HEAVY(has(size), "requested bytes are unavailable");
	return _bytes.subspan(_cursor, size);
}

std::span<const std::byte> MemoryStreamAdapter::peek(
	std::size_t left, std::size_t right
) {
	CR_FAIL_FAST_ASSERT_LIGHT(
		left <= right, std::format("Range ({}, {}) is invalid", left, right)
	);
	CR_FAIL_FAST_ASSERT_HEAVY(has(right), "range exceeds buffer");
	return _bytes.subspan(_cursor + left, right - left);
}

void MemoryStreamAdapter::take(std::size_t size, std::span<std::byte> out) {
	CR_FAIL_FAST_ASSERT_LIGHT(
		out.size() >= size,
		std::format("Output span size {} < requested size {}", out.size(), size)
	);
	const auto bytes = peek(size);
	std::memcpy(out.data(), bytes.data(), size);
	_cursor += size;
}

void MemoryStreamAdapter::skip(std::size_t size) {
	CR_FAIL_FAST_ASSERT_HEAVY(has(size), "requested bytes are unavailable");
	_cursor += size;
}

std::size_t MemoryStreamAdapter::position() const noexcept {
	return _cursor;
}

bool MemoryStreamAdapter::end() {
	return _cursor == _bytes.size();
}

IstreamAdapter::IstreamAdapter(std::istream &stream) noexcept
	: _stream(stream) {}

bool IstreamAdapter::has(std::size_t size) {
	return ensureBuffered(size);
}

std::span<const std::byte> IstreamAdapter::peek(std::size_t size) {
	CR_FAIL_FAST_ASSERT_HEAVY(has(size), "requested bytes are unavailable");
	return std::span<const std::byte>(_buffer).subspan(_cursor, size);
}

std::span<const std::byte> IstreamAdapter::peek(
	std::size_t left, std::size_t right
) {
	CR_FAIL_FAST_ASSERT_LIGHT(
		left <= right, std::format("Range ({}, {}) is invalid", left, right)
	);
	CR_FAIL_FAST_ASSERT_HEAVY(
		has(right), std::format("Range ({}, {}) exceeds buffer", left, right)
	);
	return std::span<const std::byte>(_buffer).subspan(
		_cursor + left, right - left
	);
}

void IstreamAdapter::take(std::size_t size, std::span<std::byte> out) {
	CR_FAIL_FAST_ASSERT_LIGHT(
		out.size() >= size,
		std::format("Output span size {} < requested size {}", out.size(), size)
	);
	const auto bytes = peek(size);
	std::memcpy(out.data(), bytes.data(), size);
	_cursor += size;
	_absolute_position += size;
	compactBuffer();
}

void IstreamAdapter::skip(std::size_t size) {
	CR_FAIL_FAST_ASSERT_HEAVY(has(size), "requested bytes are unavailable");
	_cursor += size;
	_absolute_position += size;
	compactBuffer();
}

std::size_t IstreamAdapter::position() const noexcept {
	return _absolute_position;
}

bool IstreamAdapter::end() {
	if (!_stream_ended) {
		has(1);
	}
	return _stream_ended && _cursor == _buffer.size();
}

bool IstreamAdapter::ensureBuffered(std::size_t size) {
	if (_cursor + size <= _buffer.size()) {
		return true;
	}

	if (_stream_ended) {
		return false;
	}

	while (_cursor + size > _buffer.size() && !_stream_ended) {
		std::array<char, 4096> chunk{};
		_stream.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
		const auto read_count = _stream.gcount();
		if (read_count <= 0) {
			// Currently no more data, but might get supplied later
			if (_stream.eof() || !_stream.good()) {
				_stream_ended = true;
			}
			break;
		}

		const auto *first = reinterpret_cast<const std::byte *>(chunk.data());
		const auto *last = first + read_count;
		_buffer.insert(_buffer.end(), first, last);
	}

	return _cursor + size <= _buffer.size();
}

void IstreamAdapter::compactBuffer() {
	if (_cursor == 0) {
		return;
	}

	if (_cursor * 2 < _buffer.size()) {
		return;
	}

	_buffer.erase(_buffer.begin(), _buffer.begin() + _cursor);
	_cursor = 0;
}

} // namespace cr
