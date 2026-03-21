#include "corertt/tui.h"
#include "corertt/tilemap.h"
#include "corertt/world.h"
#include <algorithm>
#include <chrono>
#include <format>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

namespace cr {

namespace {

constexpr int min_map_window_outer_width = 3;
constexpr int min_map_window_outer_height = 3;
constexpr int info_panel_preferred_min_width = 24;
constexpr std::size_t sidebar_log_limit = 18;

struct CameraState {
	int x = 0;
	int y = 0;
	int viewport_size = 1;
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

	const int reserved_info_width =
		terminal_cols > info_panel_preferred_min_width
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
			{world.width(), world.height(), map_content_max_rows, map_content_max_cols}
		)
	);
}

std::string escapeForSidebar(std::string_view input) {
	std::string escaped;
	escaped.reserve(input.size());
	for (const char ch : input) {
		switch (ch) {
		case '\n':
			escaped += "\\n";
			break;
		case '\t':
			escaped += "\\t";
			break;
		case '\r':
			escaped += "\\r";
			break;
		default:
			escaped.push_back(ch);
			break;
		}
	}
	return escaped;
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

	if (tile.occupied_state == Tile::OCCUPIED_BY_UNIT && tile.unit_ptr != nullptr) {
		glyph = tile.unit_ptr->player_id == 1 ? 'A' : 'V';
		fg = Color::RGB(255, 255, 255);
		emphasize = true;
	} else if (
		tile.occupied_state == Tile::OCCUPIED_BY_BULLET
		&& tile.bullet_ptr != nullptr
	) {
		glyph = '*';
		fg = tile.bullet_ptr->player_id == 1 ? Color::RGB(255, 120, 120)
		                                    : Color::RGB(130, 210, 255);
		emphasize = true;
	} else if (tile.is_base) {
		glyph = 'B';
		fg = tile.side == 1 ? Color::RGB(220, 90, 90) : Color::RGB(110, 170, 255);
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
			columns.push_back(renderMapCell(world, camera.x + dx, camera.y + dy));
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
	auto screen = ScreenInteractive::Fullscreen();

	std::jthread logic_thread([&](std::stop_token stop_token) {
		auto next_tick = std::chrono::steady_clock::now() + step_interval;
		while (!stop_token.stop_requested()) {
			std::this_thread::sleep_until(next_tick);
			if (stop_token.stop_requested()) {
				break;
			}
			{
				std::scoped_lock lock(world_mutex);
				world.step();
			}
			screen.PostEvent(Event::Custom);
			next_tick += step_interval;
		}
	});

	auto root = Renderer([&] {
		std::scoped_lock lock(world_mutex);
		camera.viewport_size = computeViewportSize(world);
		clampCamera(world, camera);
		const auto recent_logs = world.runtimeLogsSnapshot(sidebar_log_limit);

		std::vector<Element> log_lines;
		log_lines.reserve(recent_logs.size() + 2);
		if (recent_logs.empty()) {
			log_lines.push_back(text("(no logs yet)") | dim);
		} else {
			for (const auto &entry : recent_logs) {
				const std::string dev = entry.unit_id == 0
					? "base"
					: std::format("u{}", entry.unit_id);
				const std::string line = std::format(
					"[T{} P{}-{}] {}", entry.tick, entry.player_id, dev,
					escapeForSidebar(entry.message)
				);
				log_lines.push_back(text(line));
			}
		}

		Element map_panel = renderMapPanel(world, camera);
		Element info_panel = window(
			text("Info"),
			vbox({
				text("W/A/S/D or arrow keys: move view"),
				text("Q/Esc: quit"),
				text(std::format("Step interval: {} ms", step_interval.count())),
				separator(),
				text(std::format("Current tick: {}", world.currentTick())),
				text(std::format("View origin: ({}, {})", camera.x, camera.y)),
				separator(),
				text("Runtime logs:"),
				vbox(std::move(log_lines)) | yframe,
			})
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
