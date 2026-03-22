#include "corertt/tui.h"
#include "corertt/log.h"
#include "corertt/tilemap.h"
#include "corertt/world.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>
#include <mutex>
#include <ranges>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace cr {

namespace {

namespace UiConst {
constexpr int min_map_window_outer_width = 3;
constexpr int min_map_window_outer_height = 3;
constexpr int info_panel_preferred_min_width = 24;

constexpr char glyph_empty = ' ';
constexpr char glyph_plain = '.';
constexpr char glyph_player_1_unit = 'A';
constexpr char glyph_player_2_unit = 'V';
constexpr char glyph_bullet = '*';
constexpr char glyph_base = 'B';
constexpr char glyph_resource = '$';
constexpr char glyph_obstacle = '#';
constexpr char glyph_water = '~';

constexpr std::string_view map_title_format = "Map {}x{}  Tick {}";
constexpr std::string_view no_logs_yet = "(no logs yet)";
constexpr std::string_view status_running = "Game status: Running";
constexpr std::string_view controls_line_1 = "W/A/S/D or arrow keys: move view";
constexpr std::string_view controls_line_2 = "Q/Esc: quit";

const ftxui::Color color_plain = ftxui::Color::RGB(190, 190, 190);
const ftxui::Color color_unit = ftxui::Color::RGB(255, 255, 255);
const ftxui::Color color_bullet_p1 = ftxui::Color::RGB(255, 120, 120);
const ftxui::Color color_bullet_p2 = ftxui::Color::RGB(130, 210, 255);
const ftxui::Color color_base_p1 = ftxui::Color::RGB(220, 90, 90);
const ftxui::Color color_base_p2 = ftxui::Color::RGB(110, 170, 255);
const ftxui::Color color_resource = ftxui::Color::RGB(245, 208, 74);
const ftxui::Color color_obstacle = ftxui::Color::RGB(130, 130, 130);
const ftxui::Color color_water = ftxui::Color::RGB(95, 170, 255);
const ftxui::Color color_win = ftxui::Color::RGB(92, 201, 110);
const ftxui::Color color_loss = ftxui::Color::RGB(229, 96, 96);
} // namespace UiConst

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

	int availableRows() const noexcept {
		return content_box.y_max - content_box.y_min + 1;
	}
};

struct InfoPanelState {
	std::uint32_t tick = 0;
	int camera_x = 0;
	int camera_y = 0;
	int step_interval_ms = 0;
	bool game_over = false;
	std::uint8_t winner_player_id = 0;
	std::uint8_t captured_player_id = 0;
};

struct MapCellVisual {
	char glyph = UiConst::glyph_plain;
	ftxui::Color fg = UiConst::color_plain;
	bool emphasize = false;
};

MapCellVisual describeMapCell(const World &world, int x, int y) noexcept {
	if (!world.isInBounds(x, y)) {
		return {
			.glyph = UiConst::glyph_empty,
			.fg = UiConst::color_plain,
			.emphasize = false,
		};
	}

	const Tile tile = world.tilemap().tileOf(x, y);

	if (tile.occupied_state == Tile::OCCUPIED_BY_UNIT && tile.unit_ptr) {
		return {
			.glyph = tile.unit_ptr->player_id == 1
				? UiConst::glyph_player_1_unit
				: UiConst::glyph_player_2_unit,
			.fg = UiConst::color_unit,
			.emphasize = true,
		};
	}

	if (tile.occupied_state == Tile::OCCUPIED_BY_BULLET && tile.bullet_ptr) {
		return {
			.glyph = UiConst::glyph_bullet,
			.fg = tile.bullet_ptr->player_id == 1 ? UiConst::color_bullet_p1
												  : UiConst::color_bullet_p2,
			.emphasize = true,
		};
	}

	if (tile.is_base) {
		return {
			.glyph = UiConst::glyph_base,
			.fg = tile.side == 1 ? UiConst::color_base_p1
								 : UiConst::color_base_p2,
			.emphasize = false,
		};
	}

	if (tile.is_resource) {
		return {
			.glyph = UiConst::glyph_resource,
			.fg = UiConst::color_resource,
			.emphasize = false,
		};
	}

	if (tile.terrain == Tile::OBSTACLE) {
		return {
			.glyph = UiConst::glyph_obstacle,
			.fg = UiConst::color_obstacle,
			.emphasize = false,
		};
	}

	if (tile.terrain == Tile::WATER) {
		return {
			.glyph = UiConst::glyph_water,
			.fg = UiConst::color_water,
			.emphasize = false,
		};
	}

	return {
		.glyph = UiConst::glyph_plain,
		.fg = UiConst::color_plain,
		.emphasize = false,
	};
}

InfoPanelState buildInfoPanelState(
	const World &world, const CameraState &camera,
	std::chrono::milliseconds step_interval
) noexcept {
	return {
		.tick = world.currentTick(),
		.camera_x = camera.x,
		.camera_y = camera.y,
		.step_interval_ms = static_cast<int>(step_interval.count()),
		.game_over = world.gameOver(),
		.winner_player_id = world.winnerPlayerId(),
		.captured_player_id = world.capturedPlayerId(),
	};
}

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
		1, terminal_rows - (UiConst::min_map_window_outer_height - 1)
	);

	const int reserved_info_width = terminal_cols
			> UiConst::info_panel_preferred_min_width
		? UiConst::info_panel_preferred_min_width
		: 0;
	const int map_outer_max_width = std::max(
		UiConst::min_map_window_outer_width, terminal_cols - reserved_info_width
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

void appendRenderedLogLines(
	const LogEntry &entry, std::vector<RenderedLogLine> &out_lines
) {
	auto payload_lines = formatLogEntryLines(entry);

	// Reverse the lines
	for (auto it = payload_lines.rbegin(); it != payload_lines.rend(); ++it) {
		out_lines.push_back({*it, true});
	}
	out_lines.back().is_continuation = false;
}

std::vector<RenderedLogLine> collectVisibleLogLines(
	const World &world, int available_rows
) {
	if (available_rows <= 0) {
		return {};
	}

	std::vector<RenderedLogLine> lines;
	const auto log_range = world.runtimeLogs();
	for (auto &entry : (log_range | std::views::reverse)) {
		appendRenderedLogLines(entry, lines);
		if (lines.size() >= available_rows) {
			break;
		}
	}

	if (lines.empty()) {
		lines.push_back({std::string(UiConst::no_logs_yet), false});
		return lines;
	}

	if (lines.size() > available_rows) {
		lines.resize(available_rows);
	}
	std::reverse(lines.begin(), lines.end());
	return lines;
}

ftxui::Element renderLogPanel(const World &world, LogPanelLayoutState &layout) {
	using namespace ftxui;

	auto visible_lines = collectVisibleLogLines(world, layout.availableRows());

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

ftxui::Element renderGameStatusLine(const InfoPanelState &state) {
	using namespace ftxui;

	if (!state.game_over) {
		return text(std::string(UiConst::status_running));
	}

	Element winner = text(std::format("P{} wins", state.winner_player_id))
		| color(UiConst::color_win) | bold;
	Element
		loser = text(std::format("P{} base captured", state.captured_player_id))
		| color(UiConst::color_loss) | bold;

	return hbox({text("Game over: "), winner, text(" | "), loser});
}

ftxui::Element renderInfoPanel(
	const World &world, const CameraState &camera,
	std::chrono::milliseconds step_interval, LogPanelLayoutState &log_layout
) {
	using namespace ftxui;
	const InfoPanelState panel_state = buildInfoPanelState(
		world, camera, step_interval
	);

	Element status_panel = window(
		text("Info"),
		vbox({
			text(std::string(UiConst::controls_line_1)),
			text(std::string(UiConst::controls_line_2)),
			separator(),
			text(std::format("Current tick: {}", panel_state.tick)),
			renderGameStatusLine(panel_state),
			text(
				std::format(
					"View origin: ({}, {})", panel_state.camera_x,
					panel_state.camera_y
				)
			),
		})
	);

	return vbox({status_panel, renderLogPanel(world, log_layout) | flex});
}

ftxui::Element renderMapCell(const World &world, int x, int y) {
	using namespace ftxui;
	const MapCellVisual cell_visual = describeMapCell(world, x, y);

	Element cell = text(std::string(1, cell_visual.glyph))
		| color(cell_visual.fg);
	if (cell_visual.emphasize) {
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
			UiConst::map_title_format, world.width(), world.height(),
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
				game_over = world.gameOver();
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

		if (event == Event::q || event == Event::Q || event == Event::Escape) {
			logic_thread.request_stop();
			screen.ExitLoopClosure()();
			return true;
		}

		if (event == Event::w || event == Event::W || event == Event::ArrowUp) {
			std::scoped_lock lock(world_mutex);
			camera.y -= 1;
			clampCamera(world, camera);
			moved = true;
		}
		if (event == Event::s || event == Event::S
		    || event == Event::ArrowDown) {
			std::scoped_lock lock(world_mutex);
			camera.y += 1;
			clampCamera(world, camera);
			moved = true;
		}
		if (event == Event::a || event == Event::A
		    || event == Event::ArrowLeft) {
			std::scoped_lock lock(world_mutex);
			camera.x -= 1;
			clampCamera(world, camera);
			moved = true;
		}
		if (event == Event::d || event == Event::D
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
