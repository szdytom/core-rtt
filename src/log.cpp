#include "corertt/log.h"
#include <array>
#include <cctype>
#include <cpptrace/cpptrace.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cr {

LogEntry LogEntry::customLog(
	std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id,
	std::size_t length
) {
	auto ptr = std::make_unique<std::byte[]>(length);
	return LogEntry{
		.tick = tick,
		.player_id = player_id,
		.unit_id = unit_id,
		.source = LogSource::PLAYER,
		.type = LogType::CUSTOM,
		.payload{reinterpret_cast<const char *>(ptr.get()), length},
		.owner = std::move(ptr)
	};
}

LogEntry LogEntry::unitCreationLog(
	std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id
) {
	using namespace std::string_view_literals;
	static constexpr auto message = "Unit created"sv;

	return LogEntry{
		.tick = tick,
		.player_id = player_id,
		.unit_id = unit_id,
		.source = LogSource::SYSTEM,
		.type = LogType::UNIT_CREATION,
		.payload = message,
	};
}

LogEntry LogEntry::unitDestructionLog(
	std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id
) {
	using namespace std::string_view_literals;
	static constexpr auto message = "Unit destroyed"sv;

	return LogEntry{
		.tick = tick,
		.player_id = player_id,
		.unit_id = unit_id,
		.source = LogSource::SYSTEM,
		.type = LogType::UNIT_DESTRUCTION,
		.payload = message,
	};
}

LogEntry LogEntry::executionExceptionLog(
	std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id,
	StoppedReason reason
) {
	static constexpr std::array<std::string_view, 7> reason_strings = {
		"NOT_STOPPED",           "ABORTED",
		"PAGE_PROTECTION_FAULT", "ALIGNMENT_FAULT",
		"ILLEGAL_INSTRUCTION",   "OUT_OF_MEMORY",
		"UNKOWN_EXCEPTION"
	};

#ifndef NDEBUG
	if (std::to_underlying(reason) >= reason_strings.size()) {
		std::cerr << "Invalid StoppedReason value: "
				  << std::to_underlying(reason) << std::endl;
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
#endif

	const auto reason_str = reason_strings[std::to_underlying(reason)];
	return LogEntry{
		.tick = tick,
		.player_id = player_id,
		.unit_id = unit_id,
		.source = LogSource::SYSTEM,
		.type = LogType::EXECUTION_EXCEPTION,
		.payload = reason_str,
	};
}

LogEntry LogEntry::baseCapturedLog(
	std::uint32_t tick, std::uint8_t captured_player_id,
	std::uint8_t winner_player_id
) {
	using namespace std::string_view_literals;

#ifndef NDEBUG
	if ((captured_player_id != 1 && captured_player_id != 2)
	    || (winner_player_id != 1 && winner_player_id != 2)
	    || captured_player_id == winner_player_id) {
		std::cerr << "Invalid base capture player ids: captured="
				  << static_cast<int>(captured_player_id) << ", winner="
				  << static_cast<int>(winner_player_id) << std::endl;
		cpptrace::generate_trace().print(std::cerr);
		std::abort();
	}
#endif

	static constexpr std::array<std::string_view, 3> messages = {
		""sv,
		"Base captured, player 2 wins"sv,
		"Base captured, player 1 wins"sv,
	};

	return LogEntry{
		.tick = tick,
		.player_id = captured_player_id,
		.unit_id = 0,
		.source = LogSource::SYSTEM,
		.type = LogType::BASE_CAPTURED,
		.payload = messages[captured_player_id],
	};
}

std::vector<std::string> formatLogEntryLines(const LogEntry &entry) {
	const std::string dev_name = entry.unit_id == 0
		? "base"
		: std::format("{:02}", entry.unit_id);

	const std::string prefix = std::format(
		"[{:04} P{}-{} {}] ", entry.tick, entry.player_id, dev_name,
		entry.source == LogSource::SYSTEM ? "SYS" : "USR"
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

} // namespace cr
