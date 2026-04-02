#ifndef CORERTT_REPLAY_H
#define CORERTT_REPLAY_H

#include "corertt/buffer.h"
#include "corertt/game_rules.h"
#include "corertt/runtime.h"
#include "corertt/stream_adapter.h"
#include "corertt/tile_codec.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <iosfwd>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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

struct ReplayTilemap {
	std::uint16_t width = 0;
	std::uint16_t height = 0;
	std::uint16_t base_size = 0;
	std::vector<TileFlags> tiles;
};

struct ReplayPlayer {
	std::uint8_t id = 0;
	std::uint16_t base_energy = 0;
	std::uint8_t base_capture_counter = 0;
};

struct ReplayUnit {
	std::uint8_t id = 0;
	std::uint8_t player_id = 0;
	std::uint8_t x = 0;
	std::uint8_t y = 0;
	std::uint8_t health = 0;
	std::uint8_t attack_cooldown = 0;
	std::uint8_t upgrades = 0;
	std::uint16_t energy = 0;
};

struct ReplayBullet {
	std::uint8_t x = 0;
	std::uint8_t y = 0;
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

	static ReplayTickFrame fromWorldState(World &world);
	static std::vector<std::byte> encode(const ReplayTickFrame &tick);
};

struct ReplayHeader {
	std::uint16_t version = 5;
	ReplayTilemap tilemap;
	GameRules game_rules;

	static ReplayHeader fromWorld(const World &world);
	static std::vector<std::byte> encode(const ReplayHeader &header);
};

enum class ReplayTermination : std::uint8_t {
	Completed = 0,
	Aborted = 1,
	RuleDraw = 2,
};

struct ReplayEndMarker {
	ReplayTermination termination = ReplayTermination::Aborted;
	std::uint8_t winner_player_id = 0;

	static ReplayEndMarker aborted() noexcept;
	static ReplayEndMarker completed(std::uint8_t winner_player_id) noexcept;
	static ReplayEndMarker ruleDraw() noexcept;
	static ReplayEndMarker fromWorld(const World &world) noexcept;
	static std::vector<std::byte> encode(const ReplayEndMarker &end_marker);
};

void encodeTo(WriteBuffer &write_buffer, const ReplayHeader &header);
void encodeTo(WriteBuffer &write_buffer, const ReplayTickFrame &tick);
void encodeTo(WriteBuffer &write_buffer, const ReplayEndMarker &end_marker);

struct ReplayData {
	ReplayHeader header;
	std::vector<ReplayTickFrame> ticks;
	ReplayEndMarker end_marker;
};

enum class ReplayRecordType : std::uint8_t {
	Tick = 1,
	End = 255,
};

enum class DecodeErrorCode : std::uint8_t {
	TruncatedInput,
	BadMagic,
	UnsupportedVersion,
	TruncatedHeaderPayload,
	TruncatedTilemapHeader,
	TilemapDimensionsExceedMaximum,
	TilemapTileCountExceedsMaximum,
	TilemapDataTruncated,
	TruncatedLogEntry,
	InvalidLogSourceValue,
	InvalidLogTypeValue,
	LogPayloadTruncated,
	TickPayloadTooSmall,
	UnitCountExceedsMaximum,
	BulletCountExceedsMaximum,
	LogCountExceedsMaximum,
	LogPayloadSizeExceedsMaximum,
	TruncatedEndMarker,
	InvalidReplayTermination,
	InvalidEndMarkerPayload,
	UnknownRecordType,
	MissingHeader,
	MissingEndMarker,
};

constexpr std::string_view to_string(DecodeErrorCode code) noexcept {
	switch (code) {
	case DecodeErrorCode::TruncatedInput:
		return "TruncatedInput";
	case DecodeErrorCode::BadMagic:
		return "BadMagic";
	case DecodeErrorCode::UnsupportedVersion:
		return "UnsupportedVersion";
	case DecodeErrorCode::TruncatedHeaderPayload:
		return "TruncatedHeaderPayload";
	case DecodeErrorCode::TruncatedTilemapHeader:
		return "TruncatedTilemapHeader";
	case DecodeErrorCode::TilemapDimensionsExceedMaximum:
		return "TilemapDimensionsExceedMaximum";
	case DecodeErrorCode::TilemapTileCountExceedsMaximum:
		return "TilemapTileCountExceedsMaximum";
	case DecodeErrorCode::TilemapDataTruncated:
		return "TilemapDataTruncated";
	case DecodeErrorCode::TruncatedLogEntry:
		return "TruncatedLogEntry";
	case DecodeErrorCode::InvalidLogSourceValue:
		return "InvalidLogSourceValue";
	case DecodeErrorCode::InvalidLogTypeValue:
		return "InvalidLogTypeValue";
	case DecodeErrorCode::LogPayloadTruncated:
		return "LogPayloadTruncated";
	case DecodeErrorCode::TickPayloadTooSmall:
		return "TickPayloadTooSmall";
	case DecodeErrorCode::UnitCountExceedsMaximum:
		return "UnitCountExceedsMaximum";
	case DecodeErrorCode::BulletCountExceedsMaximum:
		return "BulletCountExceedsMaximum";
	case DecodeErrorCode::LogCountExceedsMaximum:
		return "LogCountExceedsMaximum";
	case DecodeErrorCode::LogPayloadSizeExceedsMaximum:
		return "LogPayloadSizeExceedsMaximum";
	case DecodeErrorCode::TruncatedEndMarker:
		return "TruncatedEndMarker";
	case DecodeErrorCode::InvalidReplayTermination:
		return "InvalidReplayTermination";
	case DecodeErrorCode::InvalidEndMarkerPayload:
		return "InvalidEndMarkerPayload";
	case DecodeErrorCode::UnknownRecordType:
		return "UnknownRecordType";
	case DecodeErrorCode::MissingHeader:
		return "MissingHeader";
	case DecodeErrorCode::MissingEndMarker:
		return "MissingEndMarker";
	}
	return "UnknownDecodeErrorCode";
}

template<typename T>
using DecodeResult = std::expected<T, DecodeErrorCode>;

enum class ReplayParsePhase {
	Header, // no header parsed yet
	Tick,   // header parsed, parsing tick frames
	End,    // end marker parsed, no more data expected
};

class ReplayStreamDecoder {
public:
	explicit ReplayStreamDecoder(
		std::unique_ptr<StreamAdapter> stream
	) noexcept;
	explicit ReplayStreamDecoder(std::istream &stream);

	enum class ReadStatus {
		Tick,
		End,
		Error,
	};

	struct ReadResult {
		ReadStatus status = ReadStatus::Error;
		std::optional<ReplayTickFrame> tick;
		std::optional<DecodeErrorCode> error;
	};

	bool canReadHeader();
	bool canReadNextRecord();

	bool hasHeader() const noexcept {
		return _phase != ReplayParsePhase::Header;
	}

	bool ended() const noexcept {
		return _phase == ReplayParsePhase::End;
	}

	const ReplayHeader &header() const noexcept;
	const ReplayEndMarker &endMarker() const noexcept;

	std::expected<void, DecodeErrorCode> readHeader();
	ReadResult nextTick();

private:
	ReplayParsePhase _phase = ReplayParsePhase::Header;
	std::unique_ptr<StreamAdapter> _stream;
	ReplayHeader _header;
	ReplayEndMarker _end_marker;
};

struct ReplayProgress {
	ReplayParsePhase phase = ReplayParsePhase::Header;
	ReplayHeader header;
	ReplayTickFrame current_tick;
	ReplayEndMarker end_marker;
};

} // namespace cr

template<>
struct std::formatter<cr::DecodeErrorCode> : std::formatter<std::string_view> {
	auto format(const cr::DecodeErrorCode &value, auto &ctx) const {
		return std::formatter<std::string_view>::format(
			cr::to_string(value), ctx
		);
	}
};

#endif
