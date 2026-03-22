#ifndef CORERTT_REPLAY_H
#define CORERTT_REPLAY_H

#include "corertt/runtime.h"
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iosfwd>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cr {

class World;

struct ReplayLogEntry {
	enum class Source : std::uint8_t {
		System = 0,
		Player = 1,
	};

	enum class Type : std::uint8_t {
		Custom = 0,
		UnitCreation = 1,
		UnitDestruction = 2,
		ExecutionException = 3,
		BaseCaptured = 4,
	};

	std::uint32_t tick = 0;
	std::uint8_t player_id = 0;
	std::uint8_t unit_id = 0;
	Source source = Source::System;
	Type type = Type::Custom;
	std::vector<std::byte> payload;

	static ReplayLogEntry customLog(
		std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id,
		std::size_t length
	);

	static ReplayLogEntry unitCreationLog(
		std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id
	);

	static ReplayLogEntry unitDestructionLog(
		std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id
	);

	static ReplayLogEntry executionExceptionLog(
		std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id,
		StoppedReason reason
	);

	static ReplayLogEntry baseCapturedLog(
		std::uint32_t tick, std::uint8_t winner_player_id
	);
};

std::vector<std::string> formatReplayLogEntryLines(const ReplayLogEntry &entry);

struct ReplayTile {
	std::uint8_t terrain = 0;
	std::uint8_t side = 0;
	bool is_resource = false;
	bool is_base = false;
};

struct ReplayTilemap {
	std::uint16_t width = 0;
	std::uint16_t height = 0;
	std::uint16_t base_size = 0;
	std::vector<ReplayTile> tiles;
};

struct ReplayPlayer {
	std::uint8_t id = 0;
	std::uint16_t base_energy = 0;
	std::uint8_t base_capture_counter = 0;
};

struct ReplayUnit {
	std::uint8_t id = 0;
	std::uint8_t player_id = 0;
	std::int16_t x = 0;
	std::int16_t y = 0;
	std::uint8_t health = 0;
	std::uint16_t energy = 0;
	std::uint8_t attack_cooldown = 0;
	std::uint8_t upgrades = 0;
};

struct ReplayBullet {
	std::int16_t x = 0;
	std::int16_t y = 0;
	std::uint8_t direction = 0;
	std::uint8_t player_id = 0;
	std::uint8_t damage = 0;
};

struct ReplayTickFrame {
	std::uint32_t tick = 0;
	std::array<ReplayPlayer, 2> players{};
	std::vector<ReplayUnit> units;
	std::vector<ReplayBullet> bullets;
	std::vector<ReplayLogEntry> logs;
};

struct ReplayHeader {
	std::uint16_t version = 1;
	ReplayTilemap tilemap;
};

struct ReplayData {
	ReplayHeader header;
	std::vector<ReplayTickFrame> ticks;
};

class ReplayRecorder {
public:
	explicit ReplayRecorder(const World &world);

	const ReplayHeader &header() const noexcept {
		return _header;
	}

	const std::vector<ReplayTickFrame> &ticks() const noexcept {
		return _ticks;
	}

	void addTick(World &world);
	ReplayData build() const;

private:
	ReplayHeader _header;
	std::vector<ReplayTickFrame> _ticks;
};

enum class ReplayRecordType : std::uint8_t {
	Tick = 1,
	End = 255,
};

class ReplayStreamEncoder {
public:
	ReplayStreamEncoder() noexcept = default;

	std::vector<std::byte> encodeHeader(const ReplayHeader &header);
	std::vector<std::byte> encodeTick(const ReplayTickFrame &tick);
	std::vector<std::byte> encodeEnd();

private:
	bool _header_written = false;
	bool _end_written = false;
};

class ReplayStreamDecoder {
public:
	ReplayStreamDecoder() noexcept = default;

	void pushBytes(std::span<const std::byte> bytes);
	bool hasHeader() const noexcept {
		return _has_header;
	}
	bool isEnded() const noexcept {
		return _ended;
	}

	const ReplayHeader &header() const;
	std::optional<ReplayTickFrame> tryReadNextTick();

private:
	bool _has_header = false;
	bool _ended = false;
	std::vector<std::byte> _buffer;
	std::size_t _cursor = 0;
	ReplayHeader _header;
};

class ReplayByteStream {
public:
	void pushBytes(std::vector<std::byte> bytes);
	void close() noexcept;

	bool waitPopNext(
		std::vector<std::byte> &out_chunk,
		std::chrono::milliseconds wait_timeout
	);
	bool isDrained() const noexcept;

private:
	mutable std::mutex _mutex;
	std::condition_variable _cond;
	std::deque<std::vector<std::byte>> _chunks;
	bool _closed = false;
};

void writeReplay(std::ostream &os, const ReplayData &replay);
ReplayData readReplay(std::istream &is);

} // namespace cr

#endif