#include "corertt/replay.h"
#include "corertt/entity.h"
#include "corertt/tilemap.h"
#include "corertt/world.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <format>
#include <istream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace cr {

namespace {

constexpr std::array<std::byte, 4> replay_magic = {
	std::byte{'C'}, std::byte{'R'}, std::byte{'P'}, std::byte{'L'}
};
constexpr std::uint16_t replay_version = 1;

class NeedMoreDataError : public std::runtime_error {
public:
	explicit NeedMoreDataError(const std::string &message)
		: std::runtime_error(message) {}
};

class ByteWriter {
public:
	void writeU8(std::uint8_t value) {
		_buffer.push_back(static_cast<std::byte>(value));
	}

	void writeU16(std::uint16_t value) {
		_buffer.push_back(static_cast<std::byte>(value & 0xFF));
		_buffer.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
	}

	void writeI16(std::int16_t value) {
		writeU16(static_cast<std::uint16_t>(value));
	}

	void writeU32(std::uint32_t value) {
		_buffer.push_back(static_cast<std::byte>(value & 0xFF));
		_buffer.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
		_buffer.push_back(static_cast<std::byte>((value >> 16) & 0xFF));
		_buffer.push_back(static_cast<std::byte>((value >> 24) & 0xFF));
	}

	void writeBytes(std::span<const std::byte> bytes) {
		_buffer.insert(_buffer.end(), bytes.begin(), bytes.end());
	}

	std::vector<std::byte> take() {
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

	std::uint8_t readU8() {
		require(1);
		return std::to_integer<std::uint8_t>(_bytes[_cursor++]);
	}

	std::uint16_t readU16() {
		require(2);
		const auto b0 = std::to_integer<std::uint16_t>(_bytes[_cursor++]);
		const auto b1 = std::to_integer<std::uint16_t>(_bytes[_cursor++]);
		return static_cast<std::uint16_t>(b0 | (b1 << 8));
	}

	std::int16_t readI16() {
		return static_cast<std::int16_t>(readU16());
	}

	std::uint32_t readU32() {
		require(4);
		const auto b0 = std::to_integer<std::uint32_t>(_bytes[_cursor++]);
		const auto b1 = std::to_integer<std::uint32_t>(_bytes[_cursor++]);
		const auto b2 = std::to_integer<std::uint32_t>(_bytes[_cursor++]);
		const auto b3 = std::to_integer<std::uint32_t>(_bytes[_cursor++]);
		return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
	}

	std::span<const std::byte> readBytes(std::size_t size) {
		require(size);
		auto res = _bytes.subspan(_cursor, size);
		_cursor += size;
		return res;
	}

private:
	void require(std::size_t size) const {
		if (_cursor + size > _bytes.size()) {
			throw NeedMoreDataError("Replay decode failed: truncated input");
		}
	}

	std::span<const std::byte> _bytes;
	std::size_t _cursor;
};

std::size_t tileCount(const ReplayTilemap &tilemap) {
	return static_cast<std::size_t>(tilemap.width) * tilemap.height;
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

ReplayTilemap decodeTilemap(ByteReader &reader) {
	ReplayTilemap tilemap;
	tilemap.width = reader.readU16();
	tilemap.height = reader.readU16();
	tilemap.base_size = reader.readU16();
	reader.readU16();

	const auto count = tileCount(tilemap);
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

ReplayLogEntry decodeLogEntry(ByteReader &reader) {
	ReplayLogEntry entry;
	entry.tick = reader.readU32();
	entry.player_id = reader.readU8();
	entry.unit_id = reader.readU8();
	entry.source = static_cast<ReplayLogSource>(reader.readU8());
	entry.type = static_cast<ReplayLogType>(reader.readU8());
	const auto payload_size = reader.readU16();
	if (payload_size > 0) {
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

	writer.writeU8(static_cast<std::uint8_t>(ReplayRecordType::Tick));
	writer.writeU32(tick.tick);

	for (const auto &player : tick.players) {
		writer.writeU8(player.id);
		writer.writeU16(player.base_energy);
		writer.writeU8(player.base_capture_counter);
	}

	writer.writeU16(static_cast<std::uint16_t>(tick.units.size()));
	for (const auto &unit : tick.units) {
		writer.writeU8(unit.id);
		writer.writeU8(unit.player_id);
		writer.writeI16(unit.x);
		writer.writeI16(unit.y);
		writer.writeU8(unit.health);
		writer.writeU16(unit.energy);
		writer.writeU8(unit.attack_cooldown);
		writer.writeU8(unit.upgrades);
	}

	writer.writeU16(static_cast<std::uint16_t>(tick.bullets.size()));
	for (const auto &bullet : tick.bullets) {
		writer.writeI16(bullet.x);
		writer.writeI16(bullet.y);
		writer.writeU8(bullet.direction);
		writer.writeU8(bullet.player_id);
		writer.writeU8(bullet.damage);
	}

	writer.writeU16(static_cast<std::uint16_t>(tick.logs.size()));
	for (const auto &log : tick.logs) {
		encodeLogEntry(writer, log);
	}
}

ReplayTickFrame decodeReplayTick(ByteReader &reader) {
	ReplayTickFrame tick;
	tick.tick = reader.readU32();

	for (std::size_t i = 0; i < tick.players.size(); ++i) {
		ReplayPlayer player;
		player.id = reader.readU8();
		player.base_energy = reader.readU16();
		player.base_capture_counter = reader.readU8();
		tick.players[i] = player;
	}

	const auto unit_count = reader.readU16();
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

	const auto bullet_count = reader.readU16();
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

	const auto log_count = reader.readU16();
	tick.logs.reserve(log_count);
	for (std::uint16_t i = 0; i < log_count; ++i) {
		tick.logs.push_back(decodeLogEntry(reader));
	}

	return tick;
}

void encodeReplayHeader(ByteWriter &writer, const ReplayHeader &header) {
	writer.writeBytes(replay_magic);
	writer.writeU16(header.version);
	writer.writeU16(0);
	encodeTilemap(writer, header.tilemap);
}

ReplayHeader decodeReplayHeader(ByteReader &reader) {
	for (const auto magic : replay_magic) {
		if (reader.readU8() != std::to_integer<std::uint8_t>(magic)) {
			throw std::runtime_error("Replay decode failed: bad magic");
		}
	}

	ReplayHeader header;
	header.version = reader.readU16();
	if (header.version != replay_version) {
		throw std::runtime_error(
			std::format(
				"Replay decode failed: unsupported version {}", header.version
			)
		);
	}
	reader.readU16();
	header.tilemap = decodeTilemap(reader);
	return header;
}

std::vector<std::uint8_t> toBytes(std::string_view text) {
	std::vector<std::uint8_t> bytes;
	bytes.reserve(text.size());
	for (const auto ch : text) {
		bytes.push_back(static_cast<std::uint8_t>(ch));
	}
	return bytes;
}

} // namespace

ReplayLogEntry ReplayLogEntry::customLog(
	std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id,
	std::size_t length
) {
	ReplayLogEntry entry;
	entry.tick = tick;
	entry.player_id = player_id;
	entry.unit_id = unit_id;
	entry.source = ReplayLogSource::Player;
	entry.type = ReplayLogType::Custom;
	entry.payload.assign(length, std::uint8_t{0});
	return entry;
}

ReplayLogEntry ReplayLogEntry::unitCreationLog(
	std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id
) {
	ReplayLogEntry entry;
	entry.tick = tick;
	entry.player_id = player_id;
	entry.unit_id = unit_id;
	entry.source = ReplayLogSource::System;
	entry.type = ReplayLogType::UnitCreation;
	entry.payload = toBytes("Unit created");
	return entry;
}

ReplayLogEntry ReplayLogEntry::unitDestructionLog(
	std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id
) {
	ReplayLogEntry entry;
	entry.tick = tick;
	entry.player_id = player_id;
	entry.unit_id = unit_id;
	entry.source = ReplayLogSource::System;
	entry.type = ReplayLogType::UnitDestruction;
	entry.payload = toBytes("Unit destroyed");
	return entry;
}

ReplayLogEntry ReplayLogEntry::executionExceptionLog(
	std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id,
	StoppedReason reason
) {
	static constexpr std::array<std::string_view, 7> reason_strings = {
		"NOT_STOPPED",           "ABORTED",
		"PAGE_PROTECTION_FAULT", "ALIGNMENT_FAULT",
		"ILLEGAL_INSTRUCTION",   "OUT_OF_MEMORY",
		"UNKOWN_EXCEPTION"
	};

	const auto reason_index = std::to_underlying(reason);
	if (reason_index < 0
	    || static_cast<std::size_t>(reason_index) >= reason_strings.size()) {
		throw std::runtime_error("Replay log failed: invalid stop reason");
	}

	ReplayLogEntry entry;
	entry.tick = tick;
	entry.player_id = player_id;
	entry.unit_id = unit_id;
	entry.source = ReplayLogSource::System;
	entry.type = ReplayLogType::ExecutionException;
	entry.payload = toBytes(reason_strings[reason_index]);
	return entry;
}

ReplayLogEntry ReplayLogEntry::baseCapturedLog(
	std::uint32_t tick, std::uint8_t captured_player_id,
	std::uint8_t winner_player_id
) {
	if ((captured_player_id != 1 && captured_player_id != 2)
	    || (winner_player_id != 1 && winner_player_id != 2)
	    || captured_player_id == winner_player_id) {
		throw std::runtime_error("Replay log failed: invalid base capture ids");
	}

	static constexpr std::array<std::string_view, 3> messages = {
		"",
		"Base captured, player 2 wins",
		"Base captured, player 1 wins",
	};

	ReplayLogEntry entry;
	entry.tick = tick;
	entry.player_id = captured_player_id;
	entry.unit_id = 0;
	entry.source = ReplayLogSource::System;
	entry.type = ReplayLogType::BaseCaptured;
	entry.payload = toBytes(messages[captured_player_id]);
	return entry;
}

std::string ReplayLogEntry::payloadAsString() const {
	std::string text;
	text.reserve(payload.size());
	for (const auto byte : payload) {
		text.push_back(static_cast<char>(byte));
	}
	return text;
}

std::vector<std::string> formatReplayLogEntryLines(
	const ReplayLogEntry &entry
) {
	const std::string dev_name = entry.unit_id == 0
		? "base"
		: std::format("{:02}", entry.unit_id);

	const std::string prefix = std::format(
		"[{:04} P{}-{} {}] ", entry.tick, entry.player_id, dev_name,
		entry.source == ReplayLogSource::System ? "SYS" : "USR"
	);

	std::vector<std::string> lines;
	const auto prefix_length = prefix.size();
	lines.emplace_back(prefix);

	bool pop_last = false;
	for (std::size_t i = 0; i < entry.payload.size(); ++i) {
		const auto ch = static_cast<unsigned char>(entry.payload[i]);

		if (ch == '\n') {
			pop_last = true;
			lines.emplace_back(prefix_length, ' ');
			continue;
		}

		pop_last = false;
		if (ch == '\t' || ch == '\r' || std::isprint(ch)) {
			lines.back().push_back(static_cast<char>(ch));
			continue;
		}

		lines.back() += std::format("\\x{:02X}", static_cast<int>(ch));
	}

	if (pop_last) {
		lines.pop_back();
	}

	return lines;
}

ReplayRecorder::ReplayRecorder(const World &world) {
	_header.version = replay_version;
	const auto &tilemap = world.tilemap();
	_header.tilemap.width = static_cast<std::uint16_t>(tilemap.width());
	_header.tilemap.height = static_cast<std::uint16_t>(tilemap.height());
	_header.tilemap.base_size = static_cast<std::uint16_t>(tilemap.baseSize());
	_header.tilemap.tiles.reserve(
		static_cast<std::size_t>(tilemap.width()) * tilemap.height()
	);

	for (int y = 0; y < tilemap.height(); ++y) {
		for (int x = 0; x < tilemap.width(); ++x) {
			const auto tile = tilemap.tileOf(x, y);
			_header.tilemap.tiles.push_back({
				.terrain = tile.terrain,
				.side = tile.side,
				.is_resource = tile.is_resource,
				.is_base = tile.is_base,
			});
		}
	}
}

void ReplayRecorder::addTick(World &world) {
	ReplayTickFrame tick;
	tick.tick = world.currentTick();

	for (std::size_t i = 0; i < tick.players.size(); ++i) {
		const auto player_id = static_cast<int>(i + 1);
		const auto &player = world.player(player_id);
		tick.players[i] = {
			.id = static_cast<std::uint8_t>(player_id),
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
	_ticks.push_back(std::move(tick));
}

ReplayData ReplayRecorder::build() const {
	ReplayData data;
	data.header = _header;
	data.ticks = _ticks;
	return data;
}

std::vector<std::byte> ReplayStreamEncoder::encodeHeader(
	const ReplayHeader &header
) {
	if (_header_written) {
		throw std::runtime_error(
			"Replay encode failed: header already written"
		);
	}
	if (_end_written) {
		throw std::runtime_error("Replay encode failed: stream already ended");
	}

	ByteWriter writer;
	encodeReplayHeader(writer, header);
	_header_written = true;
	return writer.take();
}

std::vector<std::byte> ReplayStreamEncoder::encodeTick(
	const ReplayTickFrame &tick
) {
	if (!_header_written) {
		throw std::runtime_error("Replay encode failed: header required first");
	}
	if (_end_written) {
		throw std::runtime_error("Replay encode failed: stream already ended");
	}

	ByteWriter writer;
	encodeReplayTick(writer, tick);
	return writer.take();
}

std::vector<std::byte> ReplayStreamEncoder::encodeEnd() {
	if (!_header_written) {
		throw std::runtime_error("Replay encode failed: header required first");
	}
	if (_end_written) {
		return {};
	}
	ByteWriter writer;
	writer.writeU8(std::to_underlying(ReplayRecordType::End));
	_end_written = true;
	return writer.take();
}

void ReplayStreamDecoder::pushBytes(std::span<const std::byte> bytes) {
	_buffer.insert(_buffer.end(), bytes.begin(), bytes.end());
}

const ReplayHeader &ReplayStreamDecoder::header() const {
	if (!_has_header) {
		throw std::runtime_error("Replay decode failed: header not available");
	}
	return _header;
}

std::optional<ReplayTickFrame> ReplayStreamDecoder::tryReadNextTick() {
	if (_ended) {
		return std::nullopt;
	}

	if (!_has_header) {
		ByteReader header_reader(
			std::span<const std::byte>(_buffer.data(), _buffer.size())
		);
		try {
			_header = decodeReplayHeader(header_reader);
			_has_header = true;
			_cursor = header_reader.cursor();
		} catch (const NeedMoreDataError &) {
			return std::nullopt;
		}
	}

	if (_cursor >= _buffer.size()) {
		return std::nullopt;
	}

	ByteReader reader(
		std::span<const std::byte>(_buffer.data(), _buffer.size())
			.subspan(_cursor)
	);

	ReplayTickFrame tick;
	try {
		const auto type = static_cast<ReplayRecordType>(reader.readU8());
		if (type == ReplayRecordType::End) {
			_ended = true;
			_cursor += reader.cursor();
			return std::nullopt;
		}

		if (type != ReplayRecordType::Tick) {
			throw std::runtime_error(
				"Replay decode failed: unknown record type"
			);
		}
		tick = decodeReplayTick(reader);
	} catch (const NeedMoreDataError &) {
		return std::nullopt;
	}

	_cursor += reader.cursor();
	if (_cursor > 0 && _cursor * 2 >= _buffer.size()) {
		_buffer.erase(
			_buffer.begin(),
			_buffer.begin() + static_cast<std::ptrdiff_t>(_cursor)
		);
		_cursor = 0;
	}
	return tick;
}

void writeReplay(std::ostream &os, const ReplayData &replay) {
	ReplayStreamEncoder encoder;
	const auto header = encoder.encodeHeader(replay.header);
	os.write(
		reinterpret_cast<const char *>(header.data()),
		static_cast<std::streamsize>(header.size())
	);

	for (const auto &tick : replay.ticks) {
		const auto bytes = encoder.encodeTick(tick);
		os.write(
			reinterpret_cast<const char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}

	const auto end = encoder.encodeEnd();
	os.write(
		reinterpret_cast<const char *>(end.data()),
		static_cast<std::streamsize>(end.size())
	);
	if (!os.good()) {
		throw std::runtime_error("Failed to write replay stream");
	}
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
	auto tick = decoder.tryReadNextTick();
	if (!decoder.hasHeader()) {
		throw std::runtime_error("Replay decode failed: missing header");
	}
	replay.header = decoder.header();

	while (tick.has_value()) {
		replay.ticks.push_back(std::move(*tick));
		tick = decoder.tryReadNextTick();
	}

	if (!decoder.isEnded()) {
		throw std::runtime_error("Replay decode failed: missing end marker");
	}

	return replay;
}

void ReplayByteStream::pushBytes(std::vector<std::byte> bytes) {
	if (bytes.empty()) {
		return;
	}

	{
		std::scoped_lock lock(_mutex);
		if (_closed) {
			throw std::runtime_error(
				"Replay byte stream failed: cannot push to closed stream"
			);
		}
		_chunks.push_back(std::move(bytes));
	}
	_cond.notify_one();
}

void ReplayByteStream::close() noexcept {
	{
		std::scoped_lock lock(_mutex);
		_closed = true;
	}
	_cond.notify_all();
}

bool ReplayByteStream::waitPopNext(
	std::vector<std::byte> &out_chunk, std::chrono::milliseconds wait_timeout
) {
	out_chunk.clear();

	std::unique_lock lock(_mutex);
	if (_chunks.empty() && !_closed) {
		_cond.wait_for(lock, wait_timeout, [&] {
			return !_chunks.empty() || _closed;
		});
	}

	if (_chunks.empty()) {
		return false;
	}

	out_chunk = std::move(_chunks.front());
	_chunks.pop_front();
	return true;
}

bool ReplayByteStream::isDrained() const noexcept {
	std::scoped_lock lock(_mutex);
	return _closed && _chunks.empty();
}

} // namespace cr
