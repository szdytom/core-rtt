#ifndef CORERTT_LOG_H
#define CORERTT_LOG_H

#include "corertt/runtime.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace cr {

enum class LogSource : std::uint8_t {
	SYSTEM,
	PLAYER,
};

enum class LogType : std::uint8_t {
	CUSTOM,
	UNIT_CREATION,
	UNIT_DESTRUCTION,
	EXECUTION_EXCEPTION,
};

struct LogEntry {
	std::uint32_t tick;
	std::uint8_t player_id; // 1 or 2
	std::uint8_t unit_id;
	LogSource source;
	LogType type;
	std::string_view payload;
	// Owner of the payload data, if needed
	std::unique_ptr<std::byte[]> owner;

	// Payload will be allocated, caller is responsible for filling it
	static LogEntry customLog(
		std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id,
		std::size_t length
	);

	static LogEntry unitCreationLog(
		std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id
	);

	static LogEntry unitDestructionLog(
		std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id
	);

	static LogEntry executionExceptionLog(
		std::uint32_t tick, std::uint8_t player_id, std::uint8_t unit_id,
		StoppedReason reason
	);
};

} // namespace cr

#endif
