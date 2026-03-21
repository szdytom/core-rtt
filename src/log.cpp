#include "corertt/log.h"
#include <array>
#include <cpptrace/cpptrace.hpp>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>

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

} // namespace cr
