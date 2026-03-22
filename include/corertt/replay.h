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
#include <iterator>
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
class FormatReplayLogEntryLines {
public:
	explicit FormatReplayLogEntryLines(const ReplayLogEntry &entry);

	class iterator {
	public:
		using iterator_category = std::input_iterator_tag;
		using value_type = std::string;
		using difference_type = std::ptrdiff_t;
		using pointer = const std::string *;
		using reference = const std::string &;

		iterator() noexcept;
		reference operator*() const noexcept;
		pointer operator->() const noexcept;
		iterator &operator++();
		iterator operator++(int);
		bool operator==(const iterator &other) const noexcept;
		bool operator!=(const iterator &other) const noexcept;

	private:
		friend class FormatReplayLogEntryLines;

		iterator(
			const ReplayLogEntry *entry, std::string prefix, bool is_begin
		);
		void buildCurrentLine();

		const ReplayLogEntry *_entry = nullptr;
		std::string _prefix;
		std::size_t _prefix_length = 0;
		std::size_t _payload_pos = 0;
		std::string _current;
		bool _is_first_line = true;
		bool _done = true;
	};

	iterator begin() const;
	iterator end() const;

private:
	static std::string computePrefix(const ReplayLogEntry &entry);

	const ReplayLogEntry &_entry;
	std::string _prefix;
};

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
	std::uint16_t version = 2;
	ReplayTilemap tilemap;
};

enum class ReplayTermination : std::uint8_t {
	Completed = 0,
	Aborted = 1,
};

struct ReplayEndMarker {
	ReplayTermination termination = ReplayTermination::Aborted;
	std::uint8_t winner_player_id = 0;
	std::uint8_t captured_player_id = 0;
};

struct ReplayData {
	ReplayHeader header;
	std::vector<ReplayTickFrame> ticks;
	ReplayEndMarker end_marker;
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
	std::vector<std::byte> encodeEnd(const ReplayEndMarker &end_marker);

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
	const ReplayEndMarker &endMarker() const;
	std::optional<ReplayTickFrame> tryReadNextTick();

private:
	bool _has_header = false;
	bool _ended = false;
	std::vector<std::byte> _buffer;
	std::size_t _cursor = 0;
	ReplayHeader _header;
	std::optional<ReplayEndMarker> _end_marker;
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