#include "corertt/replay.h"
#include "corertt/entity.h"
#include "corertt/tilemap.h"
#include "corertt/world.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <expected>
#include <format>
#include <istream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace cr {

namespace {

constexpr std::array<std::byte, 4> replay_magic = {
	std::byte{'C'}, std::byte{'R'}, std::byte{'P'}, std::byte{'L'}
};
constexpr std::uint16_t replay_version = 4;

class ByteWriter {
public:
	void writeU8(std::uint8_t value) noexcept {
		_buffer.push_back(static_cast<std::byte>(value));
	}

	void writeU16(std::uint16_t value) noexcept {
		_buffer.push_back(static_cast<std::byte>(value & 0xFF));
		_buffer.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
	}

	void writeI16(std::int16_t value) noexcept {
		writeU16(static_cast<std::uint16_t>(value));
	}

	void writeU32(std::uint32_t value) noexcept {
		_buffer.push_back(static_cast<std::byte>(value & 0xFF));
		_buffer.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
		_buffer.push_back(static_cast<std::byte>((value >> 16) & 0xFF));
		_buffer.push_back(static_cast<std::byte>((value >> 24) & 0xFF));
	}

	void writeBytes(std::span<const std::byte> bytes) noexcept {
		_buffer.insert(_buffer.end(), bytes.begin(), bytes.end());
	}

	std::vector<std::byte> take() noexcept {
		return std::move(_buffer);
	}

private:
	std::vector<std::byte> _buffer;
};

class ByteReader {
public:
	explicit ByteReader(std::span<const std::byte> bytes) noexcept
		: _bytes(bytes), _cursor(0) {}

	std::size_t remaining() const noexcept {
		return _bytes.size() - _cursor;
	}

	std::size_t cursor() const noexcept {
		return _cursor;
	}

	bool has(std::size_t size) const noexcept {
		return _cursor + size <= _bytes.size();
	}

	std::uint8_t readU8() noexcept {
		return std::to_integer<std::uint8_t>(_bytes[_cursor++]);
	}

	std::uint16_t readU16() noexcept {
		const auto b0 = std::to_integer<std::uint16_t>(_bytes[_cursor++]);
		const auto b1 = std::to_integer<std::uint16_t>(_bytes[_cursor++]);
		return static_cast<std::uint16_t>(b0 | (b1 << 8));
	}

	std::int16_t readI16() noexcept {
		return static_cast<std::int16_t>(readU16());
	}

	std::uint32_t readU32() noexcept {
		const auto b0 = std::to_integer<std::uint32_t>(_bytes[_cursor++]);
		const auto b1 = std::to_integer<std::uint32_t>(_bytes[_cursor++]);
		const auto b2 = std::to_integer<std::uint32_t>(_bytes[_cursor++]);
		const auto b3 = std::to_integer<std::uint32_t>(_bytes[_cursor++]);
		return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
	}

	std::span<const std::byte> readBytes(std::size_t size) noexcept {
		auto res = _bytes.subspan(_cursor, size);
		_cursor += size;
		return res;
	}

	bool skip(std::size_t size) noexcept {
		if (!has(size)) {
			return false;
		}
		_cursor += size;
		return true;
	}

	std::span<const std::byte> _bytes;
	std::size_t _cursor;
};

DecodeError needMoreData(DecodeErrorCode code) noexcept {
	return DecodeError{
		.kind = DecodeErrorKind::NeedMoreData,
		.code = code,
	};
}

DecodeError formatError(DecodeErrorCode code) noexcept {
	return DecodeError{
		.kind = DecodeErrorKind::FormatError,
		.code = code,
	};
}

std::size_t tileCount(const ReplayTilemap &tilemap) noexcept {
	return static_cast<std::size_t>(tilemap.width) * tilemap.height;
}

std::size_t tilemapEncodedSize(const ReplayTilemap &tilemap) noexcept {
	return 8 + tileCount(tilemap);
}

void encodeTilemap(ByteWriter &writer, const ReplayTilemap &tilemap) {
	writer.writeU16(tilemap.width);
	writer.writeU16(tilemap.height);
	writer.writeU16(tilemap.base_size);
	writer.writeU16(0);

	if (tilemap.tiles.size() != tileCount(tilemap)) {
		throw std::runtime_error("Replay encode failed: tile count mismatch");
	}

	for (const auto &tile : tilemap.tiles) {
		std::uint8_t packed = 0;
		packed |= static_cast<std::uint8_t>(tile.terrain & 0b11);
		packed |= static_cast<std::uint8_t>((tile.side & 0b11) << 2);
		packed |= static_cast<std::uint8_t>(tile.is_resource ? 0b00010000 : 0);
		packed |= static_cast<std::uint8_t>(tile.is_base ? 0b00100000 : 0);
		writer.writeU8(packed);
	}
}

DecodeResult<ReplayTilemap> decodeTilemap(
	ByteReader &reader, bool short_is_format
) noexcept {
	auto short_error = [&](DecodeErrorCode code) {
		return short_is_format ? formatError(code) : needMoreData(code);
	};

	ReplayTilemap tilemap;
	if (!reader.has(8)) {
		return std::unexpected(
			short_error(DecodeErrorCode::TruncatedTilemapHeader)
		);
	}
	tilemap.width = reader.readU16();
	tilemap.height = reader.readU16();
	tilemap.base_size = reader.readU16();
	reader.skip(2);

	constexpr std::uint32_t max_tile_dimension = 256;
	constexpr std::uint32_t max_tile_count = max_tile_dimension
		* max_tile_dimension;
	if (tilemap.width > max_tile_dimension
	    || tilemap.height > max_tile_dimension) {
		return std::unexpected(
			formatError(DecodeErrorCode::TilemapDimensionsExceedMaximum)
		);
	}
	const auto count = tileCount(tilemap);
	if (count > max_tile_count) {
		return std::unexpected(
			formatError(DecodeErrorCode::TilemapTileCountExceedsMaximum)
		);
	}
	if (!reader.has(count)) {
		return std::unexpected(
			short_error(DecodeErrorCode::TilemapDataTruncated)
		);
	}
	tilemap.tiles.reserve(count);
	for (std::size_t i = 0; i < count; ++i) {
		const auto packed = reader.readU8();
		ReplayTile tile;
		tile.terrain = packed & 0b11;
		tile.side = (packed >> 2) & 0b11;
		tile.is_resource = (packed & 0b00010000) != 0;
		tile.is_base = (packed & 0b00100000) != 0;
		tilemap.tiles.push_back(tile);
	}
	return tilemap;
}

void encodeLogEntry(ByteWriter &writer, const ReplayLogEntry &entry) {
	if (entry.payload.size() > std::numeric_limits<std::uint16_t>::max()) {
		throw std::runtime_error("Replay encode failed: log payload too large");
	}

	writer.writeU32(entry.tick);
	writer.writeU8(entry.player_id);
	writer.writeU8(entry.unit_id);
	writer.writeU8(static_cast<std::uint8_t>(entry.source));
	writer.writeU8(static_cast<std::uint8_t>(entry.type));
	writer.writeU16(static_cast<std::uint16_t>(entry.payload.size()));
	if (!entry.payload.empty()) {
		writer.writeBytes(std::as_bytes(std::span(entry.payload)));
	}
}

DecodeResult<ReplayLogEntry> decodeLogEntry(
	ByteReader &reader, bool short_is_format
) noexcept {
	auto short_error = [&](DecodeErrorCode code) {
		return short_is_format ? formatError(code) : needMoreData(code);
	};

	ReplayLogEntry entry;
	if (!reader.has(10)) {
		return std::unexpected(short_error(DecodeErrorCode::TruncatedLogEntry));
	}
	entry.tick = reader.readU32();
	entry.player_id = reader.readU8();
	entry.unit_id = reader.readU8();

	const auto raw_source = reader.readU8();
	if (raw_source > std::to_underlying(ReplayLogEntry::Source::Player)) {
		return std::unexpected(
			formatError(DecodeErrorCode::InvalidLogSourceValue)
		);
	}
	entry.source = static_cast<ReplayLogEntry::Source>(raw_source);

	const auto raw_type = reader.readU8();
	if (raw_type > std::to_underlying(ReplayLogEntry::Type::BaseCaptured)) {
		return std::unexpected(
			formatError(DecodeErrorCode::InvalidLogTypeValue)
		);
	}
	entry.type = static_cast<ReplayLogEntry::Type>(raw_type);

	const auto payload_size = reader.readU16();
	if (payload_size > 0) {
		if (!reader.has(payload_size)) {
			return std::unexpected(
				short_error(DecodeErrorCode::LogPayloadTruncated)
			);
		}
		const auto bytes = reader.readBytes(payload_size);
		entry.payload.resize(payload_size);
		std::memcpy(entry.payload.data(), bytes.data(), payload_size);
	}
	return entry;
}

void encodeReplayTick(ByteWriter &writer, const ReplayTickFrame &tick) {
	if (tick.units.size() > std::numeric_limits<std::uint16_t>::max()) {
		throw std::runtime_error("Replay encode failed: too many units");
	}
	if (tick.bullets.size() > std::numeric_limits<std::uint16_t>::max()) {
		throw std::runtime_error("Replay encode failed: too many bullets");
	}
	if (tick.logs.size() > std::numeric_limits<std::uint16_t>::max()) {
		throw std::runtime_error("Replay encode failed: too many logs");
	}

	ByteWriter payload;

	for (const auto &player : tick.players) {
		payload.writeU8(player.id);
		payload.writeU16(player.base_energy);
		payload.writeU8(player.base_capture_counter);
	}

	payload.writeU16(static_cast<std::uint16_t>(tick.units.size()));
	for (const auto &unit : tick.units) {
		payload.writeU8(unit.id);
		payload.writeU8(unit.player_id);
		payload.writeI16(unit.x);
		payload.writeI16(unit.y);
		payload.writeU8(unit.health);
		payload.writeU16(unit.energy);
		payload.writeU8(unit.attack_cooldown);
		payload.writeU8(unit.upgrades);
	}

	payload.writeU16(static_cast<std::uint16_t>(tick.bullets.size()));
	for (const auto &bullet : tick.bullets) {
		payload.writeI16(bullet.x);
		payload.writeI16(bullet.y);
		payload.writeU8(bullet.direction);
		payload.writeU8(bullet.player_id);
		payload.writeU8(bullet.damage);
	}

	payload.writeU16(static_cast<std::uint16_t>(tick.logs.size()));
	for (const auto &log : tick.logs) {
		encodeLogEntry(payload, log);
	}

	auto payload_bytes = payload.take();
	if (payload_bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
		throw std::runtime_error(
			"Replay encode failed: tick payload too large"
		);
	}

	writer.writeU8(static_cast<std::uint8_t>(ReplayRecordType::Tick));
	writer.writeU32(tick.tick);
	writer.writeU32(static_cast<std::uint32_t>(payload_bytes.size()));
	if (!payload_bytes.empty()) {
		writer.writeBytes(payload_bytes);
	}
}

DecodeResult<ReplayTickFrame> decodeReplayTickPayload(
	ByteReader &reader, std::uint32_t tick_id
) noexcept {
	auto fail_format = [&](DecodeErrorCode code) {
		return std::unexpected(formatError(code));
	};

	ReplayTickFrame tick;
	tick.tick = tick_id;

	constexpr std::size_t player_bytes = 2 * 4;
	if (!reader.has(player_bytes)) {
		return fail_format(DecodeErrorCode::TickPayloadTooSmall);
	}
	for (std::size_t i = 0; i < tick.players.size(); ++i) {
		ReplayPlayer player;
		player.id = reader.readU8();
		player.base_energy = reader.readU16();
		player.base_capture_counter = reader.readU8();
		tick.players[i] = player;
	}

	constexpr std::uint16_t max_unit_count = 30;
	if (!reader.has(2)) {
		return fail_format(DecodeErrorCode::TickPayloadTooSmall);
	}
	const auto unit_count = reader.readU16();
	if (unit_count > max_unit_count) {
		return fail_format(DecodeErrorCode::UnitCountExceedsMaximum);
	}

	constexpr std::size_t unit_bytes = 11;
	const auto unit_section_bytes = static_cast<std::size_t>(unit_count)
		* unit_bytes;
	if (!reader.has(unit_section_bytes)) {
		return fail_format(DecodeErrorCode::TickPayloadTooSmall);
	}

	tick.units.reserve(unit_count);
	for (std::uint16_t i = 0; i < unit_count; ++i) {
		ReplayUnit unit;
		unit.id = reader.readU8();
		unit.player_id = reader.readU8();
		unit.x = reader.readI16();
		unit.y = reader.readI16();
		unit.health = reader.readU8();
		unit.energy = reader.readU16();
		unit.attack_cooldown = reader.readU8();
		unit.upgrades = reader.readU8();
		tick.units.push_back(unit);
	}

	constexpr std::uint16_t max_bullet_count = 4096;
	if (!reader.has(2)) {
		return fail_format(DecodeErrorCode::TickPayloadTooSmall);
	}
	const auto bullet_count = reader.readU16();
	if (bullet_count > max_bullet_count) {
		return fail_format(DecodeErrorCode::BulletCountExceedsMaximum);
	}

	constexpr std::size_t bullet_bytes = 7;
	const auto bullet_section_bytes = static_cast<std::size_t>(bullet_count)
		* bullet_bytes;
	if (!reader.has(bullet_section_bytes)) {
		return fail_format(DecodeErrorCode::TickPayloadTooSmall);
	}

	tick.bullets.reserve(bullet_count);
	for (std::uint16_t i = 0; i < bullet_count; ++i) {
		ReplayBullet bullet;
		bullet.x = reader.readI16();
		bullet.y = reader.readI16();
		bullet.direction = reader.readU8();
		bullet.player_id = reader.readU8();
		bullet.damage = reader.readU8();
		tick.bullets.push_back(bullet);
	}

	constexpr std::uint16_t max_log_count = 64;
	if (!reader.has(2)) {
		return fail_format(DecodeErrorCode::TickPayloadTooSmall);
	}
	const auto log_count = reader.readU16();
	if (log_count > max_log_count) {
		return fail_format(DecodeErrorCode::LogCountExceedsMaximum);
	}
	tick.logs.reserve(log_count);
	for (std::uint16_t i = 0; i < log_count; ++i) {
		auto log_entry = decodeLogEntry(reader, true);
		if (!log_entry.has_value()) {
			return std::unexpected(log_entry.error());
		}
		tick.logs.push_back(std::move(*log_entry));
	}

	return tick;
}

void encodeReplayHeader(ByteWriter &writer, const ReplayHeader &header) {
	writer.writeBytes(replay_magic);
	writer.writeU16(header.version);
	const auto header_size = tilemapEncodedSize(header.tilemap);
	if (header_size > std::numeric_limits<std::uint16_t>::max()) {
		throw std::runtime_error(
			"Replay encode failed: header payload too large"
		);
	}
	writer.writeU16(static_cast<std::uint16_t>(header_size));
	encodeTilemap(writer, header.tilemap);
}

void encodeReplayEndMarker(
	ByteWriter &writer, const ReplayEndMarker &end_marker
) {
	writer.writeU8(std::to_underlying(ReplayRecordType::End));
	writer.writeU8(std::to_underlying(end_marker.termination));
	writer.writeU8(end_marker.winner_player_id);
}

DecodeResult<ReplayEndMarker> decodeReplayEndMarker(
	ByteReader &reader
) noexcept {
	ReplayEndMarker end_marker;
	if (!reader.has(2)) {
		return std::unexpected(
			needMoreData(DecodeErrorCode::TruncatedEndMarker)
		);
	}
	const auto raw_termination = reader.readU8();
	if (raw_termination > std::to_underlying(ReplayTermination::Aborted)) {
		return std::unexpected(
			formatError(DecodeErrorCode::InvalidReplayTermination)
		);
	}

	const auto winner_player_id = reader.readU8();

	end_marker.termination = static_cast<ReplayTermination>(raw_termination);
	end_marker.winner_player_id = winner_player_id;

	switch (end_marker.termination) {
	case ReplayTermination::Completed:
		if (winner_player_id == 0 || winner_player_id > 2) {
			return std::unexpected(
				formatError(DecodeErrorCode::InvalidEndMarkerPayload)
			);
		}
		break;

	case ReplayTermination::Aborted:
		if (winner_player_id != 0) {
			return std::unexpected(
				formatError(DecodeErrorCode::InvalidEndMarkerPayload)
			);
		}
		break;
	}

	return end_marker;
}

DecodeResult<ReplayHeader> decodeReplayHeader(ByteReader &reader) noexcept {
	if (!reader.has(8)) {
		return std::unexpected(needMoreData(DecodeErrorCode::TruncatedInput));
	}

	for (const auto magic : replay_magic) {
		if (reader.readU8() != std::to_integer<std::uint8_t>(magic)) {
			return std::unexpected(formatError(DecodeErrorCode::BadMagic));
		}
	}

	ReplayHeader header;
	header.version = reader.readU16();
	const auto header_size = reader.readU16();
	if (header.version != replay_version) {
		return std::unexpected(
			formatError(DecodeErrorCode::UnsupportedVersion)
		);
	}

	if (!reader.has(header_size)) {
		return std::unexpected(
			needMoreData(DecodeErrorCode::TruncatedHeaderPayload)
		);
	}

	ByteReader header_reader(reader.readBytes(header_size));
	auto tilemap = decodeTilemap(header_reader, true);
	if (!tilemap.has_value()) {
		return std::unexpected(tilemap.error());
	}
	header.tilemap = std::move(*tilemap);
	return header;
}

std::vector<std::byte> toBytes(std::string_view text) noexcept {
	std::vector<std::byte> bytes;
	bytes.reserve(text.size());
	for (const auto ch : text) {
		bytes.push_back(static_cast<std::byte>(ch));
	}
	return bytes;
}

} // namespace

ReplayLogEntry ReplayLogEntry::customLog(
	std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id,
	std::size_t length
) {
	return {
		.tick = tick,
		.player_id = player_id,
		.unit_id = unit_id,
		.source = ReplayLogEntry::Source::Player,
		.type = ReplayLogEntry::Type::Custom,
		.payload = std::vector<std::byte>(length, std::byte{0}),
	};
}

ReplayLogEntry ReplayLogEntry::unitCreationLog(
	std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id
) {
	return {
		.tick = tick,
		.player_id = player_id,
		.unit_id = unit_id,
		.source = ReplayLogEntry::Source::System,
		.type = ReplayLogEntry::Type::UnitCreation,
		.payload = toBytes("Unit created"),
	};
}

ReplayLogEntry ReplayLogEntry::unitDestructionLog(
	std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id
) {
	return {
		.tick = tick,
		.player_id = player_id,
		.unit_id = unit_id,
		.source = ReplayLogEntry::Source::System,
		.type = ReplayLogEntry::Type::UnitDestruction,
		.payload = toBytes("Unit destroyed"),
	};
}

ReplayLogEntry ReplayLogEntry::executionExceptionLog(
	std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id,
	StoppedReason reason
) {
	static constexpr std::array<std::string_view, 7> reason_strings = {
		"NOT_STOPPED",           "ABORTED",
		"PAGE_PROTECTION_FAULT", "ALIGNMENT_FAULT",
		"ILLEGAL_INSTRUCTION",   "OUT_OF_MEMORY",
		"UNKNOWN_EXCEPTION"
	};

	const auto reason_index = std::to_underlying(reason);
	if (reason_index < 0 || reason_index >= reason_strings.size()) {
		throw std::runtime_error("Replay log failed: invalid stop reason");
	}

	return {
		.tick = tick,
		.player_id = player_id,
		.unit_id = unit_id,
		.source = ReplayLogEntry::Source::System,
		.type = ReplayLogEntry::Type::ExecutionException,
		.payload = toBytes(reason_strings[reason_index]),
	};
}

ReplayLogEntry ReplayLogEntry::baseCapturedLog(
	std::uint32_t tick, std::uint8_t winner_player_id
) {
	if (winner_player_id == 0 || winner_player_id > 2) {
		throw std::runtime_error("Replay log failed: invalid player id");
	}

	static constexpr std::array<std::string_view, 2> messages = {
		"Base captured, player 1 wins",
		"Base captured, player 2 wins",
	};

	std::uint8_t captured_player_id = 3 - winner_player_id;
	return {
		.tick = tick,
		.player_id = captured_player_id,
		.unit_id = 0, // base
		.source = ReplayLogEntry::Source::System,
		.type = ReplayLogEntry::Type::BaseCaptured,
		.payload = toBytes(messages[winner_player_id - 1]),
	};
}

FormatReplayLogEntryLines::FormatReplayLogEntryLines(
	const ReplayLogEntry &entry
)
	: _entry(entry), _prefix(computePrefix(entry)) {}

FormatReplayLogEntryLines::iterator::iterator() noexcept = default;

FormatReplayLogEntryLines::iterator::iterator(
	const ReplayLogEntry *entry, std::string prefix, bool is_begin
)
	: _entry(entry)
	, _prefix(std::move(prefix))
	, _prefix_length(_prefix.size())
	, _payload_pos(0)
	, _is_first_line(true)
	, _done(!is_begin) {
	if (_done || _entry == nullptr) {
		_done = true;
		return;
	}

	buildCurrentLine();
}

FormatReplayLogEntryLines::iterator::reference
FormatReplayLogEntryLines::iterator::operator*() const noexcept {
	return _current;
}

FormatReplayLogEntryLines::iterator::pointer
FormatReplayLogEntryLines::iterator::operator->() const noexcept {
	return &_current;
}

FormatReplayLogEntryLines::iterator &
FormatReplayLogEntryLines::iterator::operator++() {
	if (_done) {
		return *this;
	}

	if (_payload_pos >= _entry->payload.size()) {
		_done = true;
		return *this;
	}

	buildCurrentLine();
	return *this;
}

FormatReplayLogEntryLines::iterator
FormatReplayLogEntryLines::iterator::operator++(int) {
	auto temp = *this;
	++(*this);
	return temp;
}

bool FormatReplayLogEntryLines::iterator::operator==(
	const iterator &other
) const noexcept {
	if (_done && other._done) {
		return true;
	}
	if (_done != other._done) {
		return false;
	}
	return _entry == other._entry && _payload_pos == other._payload_pos
		&& _is_first_line == other._is_first_line;
}

bool FormatReplayLogEntryLines::iterator::operator!=(
	const iterator &other
) const noexcept {
	return !(*this == other);
}

void FormatReplayLogEntryLines::iterator::buildCurrentLine() {
	if (_is_first_line) {
		_current = _prefix;
		_is_first_line = false;
	} else {
		_current.assign(_prefix_length, ' ');
	}

	while (_payload_pos < _entry->payload.size()) {
		const auto ch = static_cast<unsigned char>(
			_entry->payload[_payload_pos]
		);
		++_payload_pos;

		if (ch == '\n') {
			return;
		}

		if (ch == '\t' || ch == '\r' || std::isprint(ch)) {
			_current.push_back(static_cast<char>(ch));
			continue;
		}

		_current += std::format("\\x{:02X}", static_cast<int>(ch));
	}
}

FormatReplayLogEntryLines::iterator FormatReplayLogEntryLines::begin() const {
	return iterator(&_entry, _prefix, true);
}

FormatReplayLogEntryLines::iterator FormatReplayLogEntryLines::end() const {
	return iterator();
}

std::string FormatReplayLogEntryLines::computePrefix(
	const ReplayLogEntry &entry
) {
	const std::string dev_name = entry.unit_id == 0
		? "base"
		: std::format("{:02}", entry.unit_id);

	return std::format(
		"[{:04} P{}-{} {}] ", entry.tick, entry.player_id, dev_name,
		entry.source == ReplayLogEntry::Source::System ? "SYS" : "USR"
	);
}

ReplayHeader ReplayHeader::fromWorld(const World &world) {
	ReplayHeader header;
	header.version = replay_version;
	const auto &tilemap = world.tilemap();
	header.tilemap.width = tilemap.width();
	header.tilemap.height = tilemap.height();
	header.tilemap.base_size = tilemap.baseSize();
	header.tilemap.tiles.reserve(tilemap.width() * tilemap.height());

	for (int y = 0; y < tilemap.height(); ++y) {
		for (int x = 0; x < tilemap.width(); ++x) {
			const auto tile = tilemap.tileOf(x, y);
			header.tilemap.tiles.push_back({
				.terrain = tile.terrain,
				.side = tile.side,
				.is_resource = tile.is_resource,
				.is_base = tile.is_base,
			});
		}
	}
	return header;
}

std::vector<std::byte> ReplayHeader::encode(const ReplayHeader &header) {
	ByteWriter writer;
	encodeReplayHeader(writer, header);
	return writer.take();
}

ReplayTickFrame ReplayTickFrame::fromWorldState(World &world) {
	ReplayTickFrame tick;
	tick.tick = world.currentTick();

	for (int i : {0, 1}) {
		std::uint8_t pid = i + 1;
		const auto &player = world.player(pid);
		tick.players[i] = {
			.id = pid,
			.base_energy = player.base_energy,
			.base_capture_counter = static_cast<std::uint8_t>(
				std::clamp(player.base_capture_counter, 0, 255)
			),
		};
	}

	tick.units.reserve(world._units.size());
	for (const auto &unit_ptr : world._units) {
		if (!unit_ptr || unit_ptr->health <= 0) {
			continue;
		}

		const auto &unit = *unit_ptr;
		tick.units.push_back({
			.id = unit.id,
			.player_id = unit.player_id,
			.x = unit.x,
			.y = unit.y,
			.health = unit.health,
			.energy = unit.energy,
			.attack_cooldown = unit.attack_cooldown,
			.upgrades = unit.upgrades,
		});
	}

	tick.bullets.reserve(world._bullets.size());
	for (const auto &bullet_ptr : world._bullets) {
		if (!bullet_ptr || bullet_ptr->damage <= 0) {
			continue;
		}

		const auto &bullet = *bullet_ptr;
		tick.bullets.push_back({
			.x = bullet.x,
			.y = bullet.y,
			.direction = static_cast<std::uint8_t>(bullet.direction),
			.player_id = bullet.player_id,
			.damage = bullet.damage,
		});
	}

	tick.logs = world.takeTickLogs();
	return tick;
}

std::vector<std::byte> ReplayTickFrame::encode(const ReplayTickFrame &tick) {
	ByteWriter writer;
	encodeReplayTick(writer, tick);
	return writer.take();
}

ReplayEndMarker ReplayEndMarker::fromWorld(const World &world) {
	if (world.gameOver()) {
		return ReplayEndMarker::completed(world.winnerPlayerId());
	}
	return ReplayEndMarker::aborted();
}

ReplayEndMarker ReplayEndMarker::completed(std::uint8_t winner_player_id) {
	assert(winner_player_id == 1 || winner_player_id == 2);

	return {
		.termination = ReplayTermination::Completed,
		.winner_player_id = winner_player_id
	};
}

ReplayEndMarker ReplayEndMarker::aborted() {
	return {.termination = ReplayTermination::Aborted, .winner_player_id = 0};
}

std::vector<std::byte> ReplayEndMarker::encode(
	const ReplayEndMarker &end_marker
) {
	ByteWriter writer;
	encodeReplayEndMarker(writer, end_marker);
	return writer.take();
}

void ReplayStreamDecoder::pushBytes(std::span<const std::byte> bytes) noexcept {
	_buffer.insert(_buffer.end(), bytes.begin(), bytes.end());
}

void ReplayStreamDecoder::pushBytes(std::span<const char> bytes) noexcept {
	pushBytes(
		{reinterpret_cast<const std::byte *>(bytes.data()), bytes.size()}
	);
}

bool ReplayStreamDecoder::canReadHeader() const noexcept {
	if (hasHeader()) {
		return true;
	}

	constexpr std::size_t header_prefix_size = 8;
	if (_buffer.size() < header_prefix_size) {
		return false;
	}

	const auto size_low = std::to_integer<std::uint16_t>(_buffer[6]);
	const auto size_high = std::to_integer<std::uint16_t>(_buffer[7]);
	const auto header_size = static_cast<std::size_t>(
		size_low | (size_high << 8)
	);
	return _buffer.size() >= header_prefix_size + header_size;
}

bool ReplayStreamDecoder::canReadNextRecord() const noexcept {
	if (!hasHeader() || ended()) {
		return false;
	}

	const std::size_t scan_cursor = _cursor;

	if (scan_cursor >= _buffer.size()) {
		return false;
	}

	ByteReader record_reader(
		std::span<const std::byte>(_buffer.data(), _buffer.size())
			.subspan(scan_cursor)
	);
	if (!record_reader.has(1)) {
		return false;
	}

	const auto record_type = static_cast<ReplayRecordType>(
		record_reader.readU8()
	);
	if (record_type == ReplayRecordType::End) {
		return record_reader.remaining() >= 2;
	}
	if (record_type != ReplayRecordType::Tick) {
		return false;
	}

	if (!record_reader.has(8)) {
		return false;
	}
	record_reader.readU32();
	const auto payload_size = record_reader.readU32();

	return static_cast<std::size_t>(payload_size) <= record_reader.remaining();
}

const ReplayHeader &ReplayStreamDecoder::header() const {
	if (!hasHeader()) {
		throw std::runtime_error("Replay decode failed: header not available");
	}
	return _header;
}

const ReplayEndMarker &ReplayStreamDecoder::endMarker() const {
	if (!ended()) {
		throw std::runtime_error(
			"Replay decode failed: end marker not available"
		);
	}
	return _end_marker;
}

std::expected<void, DecodeError> ReplayStreamDecoder::readHeader() noexcept {
	assert(!hasHeader());
	assert(canReadHeader());

	ByteReader reader(
		std::span<const std::byte>(_buffer.data(), _buffer.size())
	);
	auto header_result = decodeReplayHeader(reader);
	if (!header_result.has_value()) {
		return std::unexpected(header_result.error());
	}
	_header = std::move(*header_result);
	_phase = ReplayParsePhase::Tick;
	_cursor = reader.cursor();
	return {};
}

ReplayStreamDecoder::ReadResult ReplayStreamDecoder::nextTick() noexcept {
	ReadResult result;
	if (ended()) {
		result.status = ReadStatus::End;
		return result;
	}

	auto compact_buffer = [&] {
		if (_cursor > 0 && _cursor * 2 >= _buffer.size()) {
			_buffer.erase(
				_buffer.begin(),
				_buffer.begin() + static_cast<std::ptrdiff_t>(_cursor)
			);
			_cursor = 0;
		}
	};

	if (!hasHeader()) {
		ByteReader header_reader(
			std::span<const std::byte>(_buffer.data(), _buffer.size())
		);
		auto header_result = decodeReplayHeader(header_reader);
		if (!header_result.has_value()) {
			if (header_result.error().kind == DecodeErrorKind::NeedMoreData) {
				result.status = ReadStatus::NeedMoreData;
				return result;
			}
			result.status = ReadStatus::Error;
			result.error = header_result.error();
			return result;
		}

		_header = std::move(*header_result);
		_phase = ReplayParsePhase::Tick;
		_cursor = header_reader.cursor();
		compact_buffer();
	}

	if (_cursor >= _buffer.size()) {
		result.status = ReadStatus::NeedMoreData;
		return result;
	}

	ByteReader reader(
		std::span<const std::byte>(_buffer.data(), _buffer.size())
			.subspan(_cursor)
	);

	if (!reader.has(1)) {
		result.status = ReadStatus::NeedMoreData;
		return result;
	}

	const auto type = static_cast<ReplayRecordType>(reader.readU8());
	if (type == ReplayRecordType::End) {
		auto end_marker = decodeReplayEndMarker(reader);
		if (!end_marker.has_value()) {
			if (end_marker.error().kind == DecodeErrorKind::NeedMoreData) {
				result.status = ReadStatus::NeedMoreData;
				return result;
			}
			result.status = ReadStatus::Error;
			result.error = end_marker.error();
			return result;
		}

		_end_marker = std::move(*end_marker);
		_phase = ReplayParsePhase::End;
		_cursor += reader.cursor();
		compact_buffer();
		result.status = ReadStatus::End;
		return result;
	}

	if (type != ReplayRecordType::Tick) {
		result.status = ReadStatus::Error;
		result.error = formatError(DecodeErrorCode::UnknownRecordType);
		return result;
	}

	if (!reader.has(8)) {
		result.status = ReadStatus::NeedMoreData;
		return result;
	}

	const auto tick_id = reader.readU32();
	const auto payload_size = reader.readU32();

	if (static_cast<std::size_t>(payload_size) > reader.remaining()) {
		result.status = ReadStatus::NeedMoreData;
		return result;
	}

	ByteReader payload_reader(reader.readBytes(payload_size));
	auto tick = decodeReplayTickPayload(payload_reader, tick_id);
	if (!tick.has_value()) {
		result.status = ReadStatus::Error;
		result.error = tick.error();
		return result;
	}

	_cursor += reader.cursor();
	compact_buffer();
	result.status = ReadStatus::Tick;
	result.tick = std::move(*tick);
	return result;
}

ReplayData readReplay(std::istream &is) {
	std::vector<std::byte> bytes;
	std::array<char, 4096> chunk{};
	while (is.good()) {
		is.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
		const auto read_size = is.gcount();
		if (read_size <= 0) {
			break;
		}
		const auto *start = reinterpret_cast<const std::byte *>(chunk.data());
		bytes.insert(bytes.end(), start, start + read_size);
	}

	ReplayStreamDecoder decoder;
	decoder.pushBytes(bytes);

	ReplayData replay;
	bool header_captured = false;

	while (true) {
		auto read_result = decoder.nextTick();
		if (decoder.hasHeader() && !header_captured) {
			replay.header = decoder.header();
			header_captured = true;
		}
		if (read_result.status == ReplayStreamDecoder::ReadStatus::Error) {
			throw std::runtime_error(std::format("{}", *read_result.error));
		}

		if (read_result.status
		    == ReplayStreamDecoder::ReadStatus::NeedMoreData) {
			break;
		}
		if (read_result.status == ReplayStreamDecoder::ReadStatus::End) {
			break;
		}
		replay.ticks.push_back(std::move(*read_result.tick));
	}

	if (!header_captured) {
		throw std::runtime_error(
			std::format("{}", formatError(DecodeErrorCode::MissingHeader))
		);
	}

	if (!decoder.ended()) {
		throw std::runtime_error(
			std::format("{}", formatError(DecodeErrorCode::MissingEndMarker))
		);
	}
	replay.end_marker = decoder.endMarker();

	return replay;
}

} // namespace cr
