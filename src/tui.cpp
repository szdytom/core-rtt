#include "corertt/tui.h"
#include "corertt/log.h"
#include "corertt/tilemap.h"
#include "corertt/world.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <deque>
#include <format>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace cr {

namespace {

constexpr int min_map_window_outer_width = 3;
constexpr int min_map_window_outer_height = 3;
constexpr int info_panel_preferred_min_width = 24;

struct CameraState {
	int x = 0;
	int y = 0;
	int viewport_size = 1;
};

struct RenderedLogLine {
	std::string text;
	bool is_continuation;
};

struct LogPanelLayoutState {
	ftxui::Box content_box{};
	int available_rows = 1;
};

void clampCamera(const World &world, CameraState &camera) noexcept {
	const int max_x = std::max(0, world.width() - camera.viewport_size);
	const int max_y = std::max(0, world.height() - camera.viewport_size);
	camera.x = std::clamp(camera.x, 0, max_x);
	camera.y = std::clamp(camera.y, 0, max_y);
}

int computeViewportSize(const World &world) noexcept {
	const auto terminal_size = ftxui::Terminal::Size();
	const int terminal_cols = std::max(1, terminal_size.dimx);
	const int terminal_rows = std::max(1, terminal_size.dimy);

	const int map_content_max_rows = std::max(
		1, terminal_rows - (min_map_window_outer_height - 1)
	);

	const int reserved_info_width = terminal_cols
			> info_panel_preferred_min_width
		? info_panel_preferred_min_width
		: 0;
	const int map_outer_max_width = std::max(
		min_map_window_outer_width, terminal_cols - reserved_info_width
	);

	// Map content width is (2 * n - 1), and window adds 2 columns.
	const int map_content_max_cols = std::max(1, (map_outer_max_width - 1) / 2);

	return std::max(
		1,
		std::min(
			{world.width(), world.height(), map_content_max_rows,
	         map_content_max_cols}
		)
	);
}

std::vector<std::string> renderPayloadLines(std::string_view payload) {
	std::vector<std::string> lines;
	lines.emplace_back();

	for (std::size_t i = 0; i < payload.size(); ++i) {
		const unsigned char ch = static_cast<unsigned char>(payload[i]);

		if (ch == '\\' && i + 1 < payload.size()) {
			const char next = payload[i + 1];
			if (next == 'n') {
				lines.emplace_back();
				++i;
				continue;
			}
			if (next == 't') {
				lines.back().push_back('\t');
				++i;
				continue;
			}
			if (next == 'r') {
				lines.back().push_back('\r');
				++i;
				continue;
			}
		}

		if (ch == '\n') {
			lines.emplace_back();
			continue;
		}

		if (ch == '\t' || ch == '\r' || std::isprint(ch)) {
			lines.back().push_back(ch);
			continue;
		}

		lines.back() += std::format("\\x{:02X}", static_cast<int>(ch));
	}

	if (lines.empty()) {
		lines.emplace_back();
	}
	return lines;
}

std::string_view logTypeLabel(LogType type) noexcept {
	switch (type) {
	case LogType::CUSTOM:
		return "CUSTOM";
	case LogType::UNIT_CREATION:
		return "UNIT_CREATE";
	case LogType::UNIT_DESTRUCTION:
		return "UNIT_DESTROY";
	case LogType::EXECUTION_EXCEPTION:
		return "EXEC_EXIT";
	case LogType::BASE_CAPTURED:
		return "BASE_CAPTURE";
	}
	return "UNKNOWN";
}

std::vector<RenderedLogLine> formatLogEntryLines(const LogEntry &entry) {
	const std::string dev = entry.unit_id == 0
		? "base"
		: std::format("u{}", entry.unit_id);
	const std::string prefix = std::format(
		"[T{} P{}-{} {}] ", entry.tick, entry.player_id, dev,
		logTypeLabel(entry.type)
	);

	auto payload_lines = renderPayloadLines(entry.payload);
	if (payload_lines.empty()) {
		payload_lines.emplace_back();
	}

	std::vector<RenderedLogLine> formatted;
	formatted.reserve(payload_lines.size());
	formatted.push_back({prefix + payload_lines.front(), false});
	for (std::size_t i = 1; i < payload_lines.size(); ++i) {
		formatted.push_back({
			std::format("{:>{}}{}", "", prefix.size(), payload_lines[i]),
			true,
		});
	}
	return formatted;
}

std::vector<RenderedLogLine> collectVisibleLogLines(
	const World &world, int available_rows
) {
	if (available_rows <= 0) {
		return {};
	}

	std::deque<std::vector<RenderedLogLine>> selected_logs;
	int used_rows = 0;

	const auto begin = world.runtimeLogsBegin();
	for (auto it = world.runtimeLogsEnd();
	     it != begin && used_rows < available_rows;) {
		--it;
		auto lines = formatLogEntryLines(*it);
		const int line_count = static_cast<int>(lines.size());

		if (used_rows + line_count > available_rows) {
			if (used_rows == 0) {
				if (line_count > available_rows) {
					const auto keep_begin = lines.end()
						- static_cast<std::ptrdiff_t>(available_rows);
					lines = std::vector<RenderedLogLine>(
						keep_begin, lines.end()
					);
				}
				selected_logs.push_front(std::move(lines));
			}
			break;
		}

		used_rows += line_count;
		selected_logs.push_front(std::move(lines));
	}

	std::vector<RenderedLogLine> visible_lines;
	if (selected_logs.empty()) {
		visible_lines.push_back({"(no logs yet)", true});
		return visible_lines;
	}

	for (const auto &log_lines : selected_logs) {
		visible_lines.insert(
			visible_lines.end(), log_lines.begin(), log_lines.end()
		);
	}
	return visible_lines;
}

ftxui::Element renderLogPanel(const World &world, LogPanelLayoutState &layout) {
	using namespace ftxui;

	const int measured_rows = layout.content_box.y_max
		- layout.content_box.y_min + 1;
	if (measured_rows > 0) {
		layout.available_rows = measured_rows;
	}

	auto visible_lines = collectVisibleLogLines(world, layout.available_rows);

	std::vector<Element> rendered_lines;
	rendered_lines.reserve(visible_lines.size());
	for (const auto &line : visible_lines) {
		Element element = text(line.text);
		if (line.is_continuation) {
			element = element | dim;
		}
		rendered_lines.push_back(std::move(element));
	}

	Element content = vbox(std::move(rendered_lines)) | yframe
		| reflect(layout.content_box);
	return window(text("Runtime logs"), std::move(content));
}

ftxui::Element renderInfoPanel(
	const World &world, const CameraState &camera,
	std::chrono::milliseconds step_interval, LogPanelLayoutState &log_layout
) {
	using namespace ftxui;

	Element status_panel = window(
		text("Info"),
		vbox({
			text("W/A/S/D or arrow keys: move view"),
			text("Q/Esc: quit"),
			text(std::format("Step interval: {} ms", step_interval.count())),
			separator(),
			text(std::format("Current tick: {}", world.currentTick())),
			text(
				world.isGameOver()
					? std::format(
						  "Game over: P{} wins (P{} base captured)",
						  world.winnerPlayerId(), world.capturedPlayerId()
					  )
					: "Game status: Running"
			),
			text(std::format("View origin: ({}, {})", camera.x, camera.y)),
		})
	);

	return vbox({status_panel, renderLogPanel(world, log_layout) | flex});
}

ftxui::Element renderMapCell(const World &world, int x, int y) {
	using namespace ftxui;

	if (!world.isInBounds(x, y)) {
		return text(" ");
	}

	const Tile tile = world.tilemap().tileOf(x, y);
	char glyph = '.';
	Color fg = Color::RGB(190, 190, 190);
	bool emphasize = false;

	if (tile.occupied_state == Tile::OCCUPIED_BY_UNIT && tile.unit_ptr) {
		glyph = tile.unit_ptr->player_id == 1 ? 'A' : 'V';
		fg = Color::RGB(255, 255, 255);
		emphasize = true;
	} else if (
		tile.occupied_state == Tile::OCCUPIED_BY_BULLET && tile.bullet_ptr
	) {
		glyph = '*';
		fg = tile.bullet_ptr->player_id == 1
			? Color::RGB(255, 120, 120)
			: Color::RGB(130, 210, 255);
		emphasize = true;
	} else if (tile.is_base) {
		glyph = 'B';
		fg = tile.side == 1 ? Color::RGB(220, 90, 90)
							: Color::RGB(110, 170, 255);
	} else if (tile.is_resource) {
		glyph = '$';
		fg = Color::RGB(245, 208, 74);
	} else if (tile.terrain == Tile::OBSTACLE) {
		glyph = '#';
		fg = Color::RGB(130, 130, 130);
	} else if (tile.terrain == Tile::WATER) {
		glyph = '~';
		fg = Color::RGB(95, 170, 255);
	}

	Element cell = text(std::string(1, glyph)) | color(fg);
	if (emphasize) {
		cell = cell | bold;
	}
	return cell;
}

ftxui::Element renderMapPanel(const World &world, const CameraState &camera) {
	using namespace ftxui;

	std::vector<Element> rows;
	rows.reserve(camera.viewport_size);
	for (int dy = 0; dy < camera.viewport_size; ++dy) {
		std::vector<Element> columns;
		columns.reserve(camera.viewport_size * 2 - 1);
		for (int dx = 0; dx < camera.viewport_size; ++dx) {
			if (dx > 0) {
				columns.push_back(text(" "));
			}
			columns.push_back(
				renderMapCell(world, camera.x + dx, camera.y + dy)
			);
		}
		rows.push_back(hbox(std::move(columns)));
	}

	const auto title = text(
		std::format(
			"Map {}x{}  Tick {}", world.width(), world.height(),
			world.currentTick()
		)
	);

	Element body = vbox(std::move(rows))
		| size(WIDTH, EQUAL, camera.viewport_size * 2 - 1)
		| size(HEIGHT, EQUAL, camera.viewport_size);
	return window(title, body);
}

} // namespace

int runTui(World &world, std::chrono::milliseconds step_interval) {
	using namespace ftxui;

	CameraState camera;
	camera.viewport_size = computeViewportSize(world);
	clampCamera(world, camera);

	std::mutex world_mutex;
	LogPanelLayoutState log_layout;
	auto screen = ScreenInteractive::Fullscreen();

	std::jthread logic_thread([&](std::stop_token stop_token) {
		auto next_tick = std::chrono::steady_clock::now() + step_interval;
		while (!stop_token.stop_requested()) {
			std::this_thread::sleep_until(next_tick);
			if (stop_token.stop_requested()) {
				break;
			}

			bool game_over = false;
			{
				std::scoped_lock lock(world_mutex);
				world.step();
				game_over = world.isGameOver();
			}
			screen.PostEvent(Event::Custom);
			if (game_over) {
				break;
			}
			next_tick += step_interval;
		}
	});

	auto root = Renderer([&] {
		std::scoped_lock lock(world_mutex);
		camera.viewport_size = computeViewportSize(world);
		clampCamera(world, camera);

		Element map_panel = renderMapPanel(world, camera);
		Element info_panel = renderInfoPanel(
			world, camera, step_interval, log_layout
		);

		return hbox({map_panel, info_panel | flex});
	});

	root |= CatchEvent([&](Event event) {
		bool moved = false;
		if (event == Event::Custom) {
			return true;
		}

		if (event == Event::Character('q') || event == Event::Character('Q')
		    || event == Event::Escape) {
			logic_thread.request_stop();
			screen.ExitLoopClosure()();
			return true;
		}

		if (event == Event::Character('w') || event == Event::Character('W')
		    || event == Event::ArrowUp) {
			std::scoped_lock lock(world_mutex);
			camera.y -= 1;
			clampCamera(world, camera);
			moved = true;
		}
		if (event == Event::Character('s') || event == Event::Character('S')
		    || event == Event::ArrowDown) {
			std::scoped_lock lock(world_mutex);
			camera.y += 1;
			clampCamera(world, camera);
			moved = true;
		}
		if (event == Event::Character('a') || event == Event::Character('A')
		    || event == Event::ArrowLeft) {
			std::scoped_lock lock(world_mutex);
			camera.x -= 1;
			clampCamera(world, camera);
			moved = true;
		}
		if (event == Event::Character('d') || event == Event::Character('D')
		    || event == Event::ArrowRight) {
			std::scoped_lock lock(world_mutex);
			camera.x += 1;
			clampCamera(world, camera);
			moved = true;
		}
		return moved;
	});

	screen.Loop(root);
	return 0;
}

} // namespace cr
