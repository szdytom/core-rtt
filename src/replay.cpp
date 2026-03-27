#include "corertt/replay.h"
#include "corertt/entity.h"
#include "corertt/fail_fast.h"
#include "corertt/tile_codec.h"
#include "corertt/tilemap.h"
#include "corertt/world.h"
#include <algorithm>
#include <array>
#include <bit>
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

DecodeErrorCode formatError(DecodeErrorCode code) noexcept {
	return code;
}

std::size_t tileCount(const ReplayTilemap &tilemap) noexcept {
	return tilemap.width * tilemap.height;
}

std::size_t tilemapEncodedSize(const ReplayTilemap &tilemap) noexcept {
	return 8 + tileCount(tilemap);
}

void encodeTilemap(WriteBuffer &writer, const ReplayTilemap &tilemap) {
	writer.write(tilemap.width, tilemap.height, tilemap.base_size);
	writer.writeU16(0);

	if (tilemap.tiles.size() != tileCount(tilemap)) {
		CR_FAIL_FAST_ASSERT_LIGHT(
			false,
			std::format(
				"Replay encode failed: tile count mismatch (expected {}, got "
				"{})",
				tileCount(tilemap), tilemap.tiles.size()
			)
		);
	}

	for (const auto &tile : tilemap.tiles) {
		writer.writeU8(tile.pack());
	}
}

DecodeResult<ReplayTilemap> decodeTilemap(ReadBuffer &reader) {
	ReplayTilemap tilemap;
	if (!reader.has(8)) {
		return std::unexpected(
			formatError(DecodeErrorCode::TruncatedTilemapHeader)
		);
	}
	tilemap.width = reader.readU16();
	tilemap.height = reader.readU16();
	tilemap.base_size = reader.readU16();
	reader.skip(2);

	constexpr std::uint32_t MAX_TILE_DIMEMSION = 512;
	if (tilemap.width > MAX_TILE_DIMEMSION
	    || tilemap.height > MAX_TILE_DIMEMSION) {
		return std::unexpected(
			formatError(DecodeErrorCode::TilemapDimensionsExceedMaximum)
		);
	}
	const auto count = tileCount(tilemap);
	if (!reader.has(count)) {
		return std::unexpected(formatError(DecodeErrorCode::TilemapDataTruncated
		));
	}
	tilemap.tiles.reserve(count);
	for (std::size_t i = 0; i < count; ++i) {
		tilemap.tiles.push_back(TileFlags::unpack(reader.readU8()));
	}
	return tilemap;
}

void encodeLogEntry(WriteBuffer &writer, const ReplayLogEntry &entry) {
	if (entry.payload.size() > std::numeric_limits<std::uint16_t>::max()) {
		CR_FAIL_FAST_ASSERT_LIGHT(
			false, "Replay encode failed: log payload too large"
		);
	}

	writer.write(
		entry.tick, entry.player_id, entry.unit_id,
		std::to_underlying(entry.source), std::to_underlying(entry.type),
		static_cast<std::uint16_t>(entry.payload.size())
	);
	if (!entry.payload.empty()) {
		writer.writeBytes(std::as_bytes(std::span(entry.payload)));
	}
}

DecodeResult<ReplayLogEntry> decodeLogEntry(ReadBuffer &reader) {
	ReplayLogEntry entry;
	if (!reader.has(10)) {
		return std::unexpected(formatError(DecodeErrorCode::TruncatedLogEntry));
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
		return std::unexpected(formatError(DecodeErrorCode::InvalidLogTypeValue)
		);
	}
	entry.type = static_cast<ReplayLogEntry::Type>(raw_type);

	constexpr std::uint16_t max_payload_size = 512;
	const auto payload_size = reader.readU16();

	if (payload_size > max_payload_size) {
		return std::unexpected(
			formatError(DecodeErrorCode::LogPayloadSizeExceedsMaximum)
		);
	}

	if (payload_size > 0) {
		if (!reader.has(payload_size)) {
			return std::unexpected(
				formatError(DecodeErrorCode::LogPayloadTruncated)
			);
		}

		const auto bytes = reader.readBytes(payload_size);
		entry.payload.resize(payload_size);
		std::memcpy(entry.payload.data(), bytes.data(), payload_size);
	}
	return entry;
}

void encodeReplayTick(WriteBuffer &writer, const ReplayTickFrame &tick) {
	if (tick.units.size() > std::numeric_limits<std::uint16_t>::max()) {
		CR_FAIL_FAST_ASSERT_LIGHT(
			false, "Replay encode failed: too many units"
		);
	}
	if (tick.bullets.size() > std::numeric_limits<std::uint16_t>::max()) {
		CR_FAIL_FAST_ASSERT_LIGHT(
			false, "Replay encode failed: too many bullets"
		);
	}
	if (tick.logs.size() > std::numeric_limits<std::uint16_t>::max()) {
		CR_FAIL_FAST_ASSERT_LIGHT(false, "Replay encode failed: too many logs");
	}

	writer.write(std::to_underlying(ReplayRecordType::Tick), tick.tick);
	const auto payload_size_offset = writer.size();
	writer.writeU32(0);
	const auto payload_begin = writer.size();

	for (const auto &player : tick.players) {
		writer.write(
			player.id, player.base_energy, player.base_capture_counter
		);
	}

	writer.writeU16(tick.units.size());
	for (const auto &unit : tick.units) {
		writer.write(
			unit.id, unit.player_id, unit.x, unit.y, unit.health, unit.energy,
			unit.attack_cooldown, unit.upgrades
		);
	}

	writer.writeU16(tick.bullets.size());
	for (const auto &bullet : tick.bullets) {
		writer.write(
			bullet.x, bullet.y, bullet.direction, bullet.player_id,
			bullet.damage
		);
	}

	writer.writeU16(tick.logs.size());
	for (const auto &log : tick.logs) {
		encodeLogEntry(writer, log);
	}

	const auto payload_size = writer.size() - payload_begin;
	if (payload_size > std::numeric_limits<std::uint32_t>::max()) {
		CR_FAIL_FAST_ASSERT_LIGHT(
			false, "Replay encode failed: tick payload too large"
		);
	}
	writer.patchU32(payload_size_offset, payload_size);
}

DecodeResult<ReplayTickFrame> decodeReplayTickPayload(
	ReadBuffer &reader, std::uint32_t tick_id
) {
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
	const auto unit_section_bytes = unit_count * unit_bytes;
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
	const auto bullet_section_bytes = bullet_count * bullet_bytes;
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
		auto log_entry = decodeLogEntry(reader);
		if (!log_entry.has_value()) {
			return std::unexpected(log_entry.error());
		}
		tick.logs.push_back(std::move(*log_entry));
	}

	return tick;
}

void encodeReplayHeader(WriteBuffer &writer, const ReplayHeader &header) {
	writer.writeBytes(replay_magic);
	const auto header_size = tilemapEncodedSize(header.tilemap);
	if (header_size > std::numeric_limits<std::uint16_t>::max()) {
		CR_FAIL_FAST_ASSERT_LIGHT(
			false, "Replay encode failed: header payload too large"
		);
	}
	writer.write(header.version);
	writer.writeU16(header_size);
	encodeTilemap(writer, header.tilemap);
}

void encodeReplayEndMarker(
	WriteBuffer &writer, const ReplayEndMarker &end_marker
) {
	writer.write(
		std::to_underlying(ReplayRecordType::End),
		std::to_underlying(end_marker.termination), end_marker.winner_player_id
	);
}

DecodeResult<ReplayEndMarker> decodeReplayEndMarker(ReadBuffer &reader) {
	ReplayEndMarker end_marker;
	if (!reader.has(2)) {
		return std::unexpected(formatError(DecodeErrorCode::TruncatedEndMarker)
		);
	}
	const auto raw_termination = reader.readU8();
	if (raw_termination > std::to_underlying(ReplayTermination::Aborted)) {
		return std::unexpected(
			formatError(DecodeErrorCode::InvalidReplayTermination)
		);
	}

	const auto winner_player_id = reader.readU8();

	end_marker.termination = std::bit_cast<ReplayTermination>(raw_termination);
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

DecodeResult<ReplayHeader> decodeReplayHeader(ReadBuffer &reader) {
	if (!reader.has(8)) {
		return std::unexpected(formatError(DecodeErrorCode::TruncatedInput));
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
		return std::unexpected(formatError(DecodeErrorCode::UnsupportedVersion)
		);
	}

	if (!reader.has(header_size)) {
		return std::unexpected(
			formatError(DecodeErrorCode::TruncatedHeaderPayload)
		);
	}
	MemoryStreamAdapter header_stream(reader.readBytes(header_size));
	ReadBuffer header_reader(header_stream);
	auto tilemap = decodeTilemap(header_reader);
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
	const auto reason_string = std::format("{}", reason);
	if (reason_string.empty()) {
		CR_FAIL_FAST_ASSERT_LIGHT(
			false, "Replay log failed: invalid stop reason"
		);
	}

	return {
		.tick = tick,
		.player_id = player_id,
		.unit_id = unit_id,
		.source = ReplayLogEntry::Source::System,
		.type = ReplayLogEntry::Type::ExecutionException,
		.payload = toBytes(reason_string),
	};
}

ReplayLogEntry ReplayLogEntry::baseCapturedLog(
	std::uint32_t tick, std::uint8_t winner_player_id
) {
	if (winner_player_id == 0 || winner_player_id > 2) {
		CR_FAIL_FAST_ASSERT_LIGHT(
			false, "Replay log failed: invalid player id"
		);
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

FormatReplayLogEntryLines::FormatReplayLogEntryLines(const ReplayLogEntry &entry
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

bool FormatReplayLogEntryLines::iterator::operator==(const iterator &other
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

bool FormatReplayLogEntryLines::iterator::operator!=(const iterator &other
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
		const auto ch = static_cast<unsigned char>(_entry->payload[_payload_pos]
		);
		++_payload_pos;

		if (ch == '\n') {
			return;
		}

		if (ch == '\t' || ch == '\r' || std::isprint(ch)) {
			_current.push_back(ch);
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

std::string FormatReplayLogEntryLines::computePrefix(const ReplayLogEntry &entry
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
	WriteBuffer write_buffer;
	encodeTo(write_buffer, header);
	return write_buffer.take();
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
			.direction = std::to_underlying(bullet.direction),
			.player_id = bullet.player_id,
			.damage = bullet.damage,
		});
	}

	tick.logs = world.takeTickLogs();
	return tick;
}

std::vector<std::byte> ReplayTickFrame::encode(const ReplayTickFrame &tick) {
	WriteBuffer write_buffer;
	encodeTo(write_buffer, tick);
	return write_buffer.take();
}

ReplayEndMarker ReplayEndMarker::fromWorld(const World &world) noexcept {
	if (world.gameOver()) {
		return ReplayEndMarker::completed(world.winnerPlayerId());
	}
	return ReplayEndMarker::aborted();
}

ReplayEndMarker ReplayEndMarker::completed(std::uint8_t winner_player_id
) noexcept {
	CR_FAIL_FAST_ASSERT_LIGHT(
		winner_player_id == 1 || winner_player_id == 2,
		std::format("invalid winner player id: {}", winner_player_id)
	);

	return {
		.termination = ReplayTermination::Completed,
		.winner_player_id = winner_player_id
	};
}

ReplayEndMarker ReplayEndMarker::aborted() noexcept {
	return {.termination = ReplayTermination::Aborted, .winner_player_id = 0};
}

std::vector<std::byte> ReplayEndMarker::encode(const ReplayEndMarker &end_marker
) {
	WriteBuffer write_buffer;
	encodeTo(write_buffer, end_marker);
	return write_buffer.take();
}

void encodeTo(WriteBuffer &write_buffer, const ReplayHeader &header) {
	encodeReplayHeader(write_buffer, header);
}

void encodeTo(WriteBuffer &write_buffer, const ReplayTickFrame &tick) {
	encodeReplayTick(write_buffer, tick);
}

void encodeTo(WriteBuffer &write_buffer, const ReplayEndMarker &end_marker) {
	encodeReplayEndMarker(write_buffer, end_marker);
}

ReplayStreamDecoder::ReplayStreamDecoder(std::unique_ptr<StreamAdapter> stream
) noexcept
	: _stream(std::move(stream)) {
	CR_FAIL_FAST_ASSERT_LIGHT(
		_stream != nullptr, "stream adapter cannot be null"
	);
}

ReplayStreamDecoder::ReplayStreamDecoder(std::istream &stream)
	: ReplayStreamDecoder(std::make_unique<IstreamAdapter>(stream)) {}

bool ReplayStreamDecoder::canReadHeader() {
	if (hasHeader()) {
		return true;
	}

	constexpr std::size_t header_prefix_size = 8;
	if (!_stream->has(header_prefix_size)) {
		return false;
	}

	const auto prefix = _stream->peek(header_prefix_size);
	const auto size_low = std::to_integer<std::uint16_t>(prefix[6]);
	const auto size_high = std::to_integer<std::uint16_t>(prefix[7]);
	const std::size_t header_size = size_low | (size_high << 8);
	return _stream->has(header_prefix_size + header_size);
}

bool ReplayStreamDecoder::canReadNextRecord() {
	if (!hasHeader() || ended() || !_stream->has(1)) {
		return false;
	}

	const auto type = static_cast<ReplayRecordType>(
		std::to_integer<std::uint8_t>(_stream->peek(1)[0])
	);
	if (type == ReplayRecordType::End) {
		return _stream->has(2);
	}
	if (type != ReplayRecordType::Tick) {
		return false;
	}

	if (!_stream->has(9)) {
		return false;
	}
	const auto prefix = _stream->peek(9);
	const auto b0 = std::to_integer<std::uint32_t>(prefix[5]);
	const auto b1 = std::to_integer<std::uint32_t>(prefix[6]);
	const auto b2 = std::to_integer<std::uint32_t>(prefix[7]);
	const auto b3 = std::to_integer<std::uint32_t>(prefix[8]);
	const auto payload_size = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
	return _stream->has(9zu + payload_size);
}

const ReplayHeader &ReplayStreamDecoder::header() const noexcept {
	if (!hasHeader()) {
		CR_FAIL_FAST_ASSERT_LIGHT(
			false, "Replay decode failed: header not available"
		);
	}
	return _header;
}

const ReplayEndMarker &ReplayStreamDecoder::endMarker() const noexcept {
	if (!ended()) {
		CR_FAIL_FAST_ASSERT_LIGHT(
			false, "Replay decode failed: end marker not available"
		);
	}
	return _end_marker;
}

std::expected<void, DecodeErrorCode> ReplayStreamDecoder::readHeader() {
	CR_FAIL_FAST_ASSERT_LIGHT(
		!hasHeader(), "ReplayStreamDecoder::readHeader called after header read"
	);
	CR_FAIL_FAST_ASSERT_HEAVY(
		canReadHeader(),
		"ReplayStreamDecoder::readHeader called before header is available"
	);
	ReadBuffer reader(*_stream);
	auto header_result = decodeReplayHeader(reader);
	if (!header_result.has_value()) {
		return std::unexpected(header_result.error());
	}
	_header = std::move(*header_result);
	_phase = ReplayParsePhase::Tick;
	return {};
}

ReplayStreamDecoder::ReadResult ReplayStreamDecoder::nextTick() {
	ReadResult result;
	CR_FAIL_FAST_ASSERT_LIGHT(hasHeader(), "replay header not read");
	CR_FAIL_FAST_ASSERT_LIGHT(!ended(), "replay ended");
	CR_FAIL_FAST_ASSERT_HEAVY(
		canReadNextRecord(), "complete next record not available yet"
	);

	if (ended()) {
		result.status = ReadStatus::End;
		return result;
	}
	ReadBuffer reader(*_stream);
	const auto type = std::bit_cast<ReplayRecordType>(reader.readU8());
	if (type == ReplayRecordType::End) {
		auto end_marker = decodeReplayEndMarker(reader);
		if (!end_marker.has_value()) {
			result.status = ReadStatus::Error;
			result.error = end_marker.error();
			return result;
		}

		_end_marker = std::move(*end_marker);
		_phase = ReplayParsePhase::End;
		result.status = ReadStatus::End;
		return result;
	}

	if (type != ReplayRecordType::Tick) {
		result.status = ReadStatus::Error;
		result.error = formatError(DecodeErrorCode::UnknownRecordType);
		return result;
	}

	const auto tick_id = reader.readU32();
	const auto payload_size = reader.readU32();
	if (!reader.has(payload_size)) {
		result.status = ReadStatus::Error;
		result.error = formatError(DecodeErrorCode::TickPayloadTooSmall);
		return result;
	}
	MemoryStreamAdapter payload_stream(reader.peek(payload_size));
	ReadBuffer payload_reader(payload_stream);
	auto tick = decodeReplayTickPayload(payload_reader, tick_id);
	if (!tick.has_value()) {
		result.status = ReadStatus::Error;
		result.error = tick.error();
		return result;
	}
	reader.skip(payload_size);
	result.status = ReadStatus::Tick;
	result.tick = std::move(*tick);
	return result;
}

} // namespace cr
