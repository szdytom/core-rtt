#include "corertt/replay.h"
#include <argparse/argparse.hpp>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::string payloadToHex(std::span<const std::byte> payload) {
	static constexpr std::string_view digits = "0123456789ABCDEF";
	std::string hex;
	hex.reserve(payload.size() * 2);
	for (const auto raw_byte : payload) {
		auto byte = std::to_underlying(raw_byte);
		hex.push_back(digits[(byte >> 4) & 0x0F]);
		hex.push_back(digits[byte & 0x0F]);
	}
	return hex;
}

std::string_view logSourceToString(cr::ReplayLogEntry::Source source) {
	switch (source) {
	case cr::ReplayLogEntry::Source::System:
		return "SYS";
	case cr::ReplayLogEntry::Source::Player:
		return "USR";
	}
	throw std::runtime_error("Invalid replay log source");
}

std::string_view logTypeToString(cr::ReplayLogEntry::Type type) {
	switch (type) {
	case cr::ReplayLogEntry::Type::Custom:
		return "custom";
	case cr::ReplayLogEntry::Type::UnitCreation:
		return "unit_creation";
	case cr::ReplayLogEntry::Type::UnitDestruction:
		return "unit_destruction";
	case cr::ReplayLogEntry::Type::ExecutionException:
		return "execution_exception";
	case cr::ReplayLogEntry::Type::BaseCaptured:
		return "base_captured";
	}
	throw std::runtime_error("Invalid replay log type");
}

} // namespace

namespace nlohmann {

template<>
struct adl_serializer<cr::ReplayLogEntry::Source> {
	static void to_json(json &j, const cr::ReplayLogEntry::Source &source) {
		j = logSourceToString(source);
	}
};

template<>
struct adl_serializer<cr::ReplayLogEntry::Type> {
	static void to_json(json &j, const cr::ReplayLogEntry::Type &type) {
		j = logTypeToString(type);
	}
};

template<>
struct adl_serializer<cr::ReplayLogEntry> {
	static void to_json(json &j, const cr::ReplayLogEntry &entry) {
		j = json{
			{"tick", entry.tick},
			{"player_id", entry.player_id},
			{"unit_id", entry.unit_id},
			{"source", entry.source},
			{"type", entry.type},
			{"payload_text",
		     std::string_view(
				 reinterpret_cast<const char *>(entry.payload.data()),
				 entry.payload.size()
			 )},
			{"payload_hex", payloadToHex(entry.payload)},
		};
	}
};

} // namespace nlohmann

int main(int argc, char *argv[]) {
	argparse::ArgumentParser program("corertt_replay_log");
	program.add_argument("--replay-file")
		.required()
		.help("input replay file path");
	program.add_argument("--output")
		.default_value(std::string(""))
		.help("output file path, stdout when omitted");
	program.add_argument("--format")
		.default_value(std::string("jsonl"))
		.help("output format: jsonl or text");

	try {
		program.parse_args(argc, argv);
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		std::cerr << program;
		return 1;
	}

	const auto input_path = program.get<std::string>("--replay-file");
	const auto output_path = program.get<std::string>("--output");
	const auto format = program.get<std::string>("--format");
	if (format != "jsonl" && format != "text") {
		std::cerr << "--format must be one of: jsonl, text\n";
		return 1;
	}

	try {
		std::ifstream input_stream(input_path, std::ios::binary);
		if (!input_stream.is_open()) {
			throw std::runtime_error("Failed to open replay file");
		}
		const auto replay_data = cr::readReplay(input_stream);

		std::ofstream output_file;
		std::ostream *output_stream = &std::cout;
		if (!output_path.empty()) {
			output_file.open(output_path, std::ios::binary | std::ios::trunc);
			if (!output_file.is_open()) {
				throw std::runtime_error("Failed to open output file");
			}
			output_stream = &output_file;
		}

		for (const auto &tick : replay_data.ticks) {
			for (const auto &entry : tick.logs) {
				if (format == "text") {
					const auto lines = cr::FormatReplayLogEntryLines(entry);
					for (const auto &line : lines) {
						*output_stream << line << '\n';
					}
					continue;
				}

				const nlohmann::json json_line = entry;
				*output_stream << json_line.dump() << '\n';
			}
		}

		output_stream->flush();
		if (!output_stream->good()) {
			throw std::runtime_error("Failed to write output logs");
		}
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}

	return 0;
}
