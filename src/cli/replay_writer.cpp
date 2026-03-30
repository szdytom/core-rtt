#include "corertt/cli.h"
#include <format>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <vector>
#include <zstd.h>

namespace cr {

namespace {

void validateZstdCompressionLevel(int compression_level) {
	const int min_level = ZSTD_minCLevel();
	const int max_level = ZSTD_maxCLevel();
	if (compression_level < min_level || compression_level > max_level) {
		throw std::runtime_error(
			std::format(
				"Invalid zstd compression level {} (supported range: {}..{})",
				compression_level, min_level, max_level
			)
		);
	}
}

void writeBytesToStream(
	std::ostream &stream, const char *data, std::size_t size
) {
	if (size == 0) {
		return;
	}

	stream.write(data, static_cast<std::streamsize>(size));
	if (!stream.good()) {
		throw std::runtime_error("Failed to write replay file");
	}
}

class RawReplayChunkWriter final : public ReplayChunkWriter {
public:
	explicit RawReplayChunkWriter(std::ostream &stream): _stream(stream) {}

	void writeChunk(std::span<const std::byte> chunk) override {
		if (chunk.empty()) {
			return;
		}

		writeBytesToStream(
			_stream, reinterpret_cast<const char *>(chunk.data()), chunk.size()
		);
	}

	void finish() override {
		_stream.flush();
		if (!_stream.good()) {
			throw std::runtime_error("Failed to flush replay file");
		}
	}

	~RawReplayChunkWriter() override {
		try {
			finish();
		} catch (...) {}
	}

private:
	std::ostream &_stream;
};

class ZstdReplayChunkWriter final : public ReplayChunkWriter {
public:
	ZstdReplayChunkWriter(std::ostream &stream, int compression_level)
		: _stream(stream), _out_buffer(ZSTD_CStreamOutSize()) {
		validateZstdCompressionLevel(compression_level);

		_context = ZSTD_createCStream();
		if (_context == nullptr) {
			throw std::runtime_error(
				"Failed to create zstd compression stream"
			);
		}

		const std::size_t init_result = ZSTD_initCStream(
			_context, compression_level
		);
		if (ZSTD_isError(init_result) != 0U) {
			throw std::runtime_error(
				std::format(
					"Failed to initialize zstd compression stream: {}",
					ZSTD_getErrorName(init_result)
				)
			);
		}
	}

	~ZstdReplayChunkWriter() override {
		if (_context != nullptr) {
			if (!_finished) {
				try {
					finish();
				} catch (...) {}
			}
			ZSTD_freeCStream(_context);
		}
	}

	void writeChunk(std::span<const std::byte> chunk) override {
		if (chunk.empty()) {
			return;
		}

		ZSTD_inBuffer input_buffer = {
			.src = chunk.data(),
			.size = chunk.size(),
			.pos = 0,
		};

		while (input_buffer.pos < input_buffer.size) {
			ZSTD_outBuffer output_buffer = {
				.dst = _out_buffer.data(),
				.size = _out_buffer.size(),
				.pos = 0,
			};

			const std::size_t compress_result = ZSTD_compressStream2(
				_context, &output_buffer, &input_buffer, ZSTD_e_continue
			);
			if (ZSTD_isError(compress_result) != 0U) {
				throw std::runtime_error(
					std::format(
						"zstd replay output failed: {}",
						ZSTD_getErrorName(compress_result)
					)
				);
			}

			writeBytesToStream(
				_stream, reinterpret_cast<const char *>(_out_buffer.data()),
				output_buffer.pos
			);
		}
	}

	void finish() override {
		if (_finished) {
			return;
		}

		ZSTD_inBuffer input_buffer = {
			.src = nullptr,
			.size = 0,
			.pos = 0,
		};

		std::size_t remaining = 1;
		while (remaining != 0) {
			ZSTD_outBuffer output_buffer = {
				.dst = _out_buffer.data(),
				.size = _out_buffer.size(),
				.pos = 0,
			};

			remaining = ZSTD_compressStream2(
				_context, &output_buffer, &input_buffer, ZSTD_e_end
			);
			if (ZSTD_isError(remaining) != 0U) {
				throw std::runtime_error(
					std::format(
						"zstd replay output failed: {}",
						ZSTD_getErrorName(remaining)
					)
				);
			}

			writeBytesToStream(
				_stream, reinterpret_cast<const char *>(_out_buffer.data()),
				output_buffer.pos
			);
		}

		_stream.flush();
		if (!_stream.good()) {
			throw std::runtime_error("Failed to flush replay file");
		}
		_finished = true;
	}

private:
	std::ostream &_stream;
	ZSTD_CStream *_context = nullptr;
	std::vector<std::byte> _out_buffer;
	bool _finished = false;
};

} // namespace

std::optional<std::ofstream> openReplayFile(const std::string &path) {
	if (path.empty()) {
		return std::nullopt;
	}

	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		throw std::runtime_error(
			std::format("Failed to open replay file: {}", path)
		);
	}
	return stream;
}

std::unique_ptr<ReplayChunkWriter> createRawReplayChunkWriter(
	std::ostream &stream
) {
	return std::make_unique<RawReplayChunkWriter>(stream);
}

std::unique_ptr<ReplayChunkWriter> createZstdReplayChunkWriter(
	std::ostream &stream, int compression_level
) {
	return std::make_unique<ZstdReplayChunkWriter>(stream, compression_level);
}

} // namespace cr
