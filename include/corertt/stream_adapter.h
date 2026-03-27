#ifndef CORERTT_STREAM_ADAPTER_H
#define CORERTT_STREAM_ADAPTER_H

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <span>
#include <vector>

namespace cr {

class StreamAdapter {
public:
	virtual ~StreamAdapter() = default;

	virtual bool has(std::size_t size) = 0;
	virtual std::span<const std::byte> peek(std::size_t size) = 0;
	virtual std::span<const std::byte> peek(
		std::size_t left, std::size_t right
	) = 0;
	virtual void take(std::size_t size, std::span<std::byte> out) = 0;
	virtual void skip(std::size_t size) = 0;
	virtual std::size_t position() const noexcept = 0;
	virtual bool end() = 0;
};

class MemoryStreamAdapter final : public StreamAdapter {
public:
	explicit MemoryStreamAdapter(std::span<const std::byte> bytes) noexcept;

	bool has(std::size_t size) override;
	std::span<const std::byte> peek(std::size_t size) override;
	std::span<const std::byte> peek(
		std::size_t left, std::size_t right
	) override;
	void take(std::size_t size, std::span<std::byte> out) override;
	void skip(std::size_t size) override;
	std::size_t position() const noexcept override;
	bool end() override;

private:
	std::span<const std::byte> _bytes;
	std::size_t _cursor = 0;
};

class IstreamAdapter final : public StreamAdapter {
public:
	explicit IstreamAdapter(std::istream &stream) noexcept;

	bool has(std::size_t size) override;
	std::span<const std::byte> peek(std::size_t size) override;
	std::span<const std::byte> peek(
		std::size_t left, std::size_t right
	) override;
	void take(std::size_t size, std::span<std::byte> out) override;
	void skip(std::size_t size) override;
	std::size_t position() const noexcept override;
	bool end() override;

private:
	bool ensureBuffered(std::size_t size);
	void compactBuffer();

	std::istream &_stream;
	std::vector<std::byte> _buffer;
	std::size_t _cursor = 0;
	std::size_t _absolute_position = 0;
	bool _stream_ended = false;
};

} // namespace cr

#endif
