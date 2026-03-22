#include "corertt/tui.h"
#include "corertt/replay.h"
#include "corertt/tilemap.h"
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
constexpr int info_panel_preferred_min_width = 28;

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
constexpr std::string_view
	waiting_for_stream = "(waiting for replay stream...)";
constexpr std::string_view no_logs_yet = "(no logs yet)";
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

struct PlaybackState {
	bool has_header = false;
	bool has_tick = false;
	bool stream_ended = false;
	ReplayHeader header;
	ReplayTickFrame current_tick;
	std::vector<ReplayLogEntry> log_history;
};

struct MapCellVisual {
	char glyph = UiConst::glyph_plain;
	ftxui::Color fg = UiConst::color_plain;
	bool emphasize = false;
};

struct InfoPanelState {
	std::uint32_t tick = 0;
	int camera_x = 0;
	int camera_y = 0;
	int step_interval_ms = 0;
	bool stream_ended = false;
	bool game_over = false;
	std::uint8_t winner_player_id = 0;
	std::uint8_t captured_player_id = 0;
};

bool isInBounds(const ReplayTilemap &tilemap, int x, int y) noexcept {
	return x >= 0 && y >= 0 && x < tilemap.width && y < tilemap.height;
}

const ReplayTile *tileAt(const ReplayTilemap &tilemap, int x, int y) noexcept {
	if (!isInBounds(tilemap, x, y)) {
		return nullptr;
	}

	const auto index = static_cast<std::size_t>(y) * tilemap.width + x;
	if (index >= tilemap.tiles.size()) {
		return nullptr;
	}
	return &tilemap.tiles[index];
}

const ReplayUnit *findUnitAt(
	const ReplayTickFrame &tick, int x, int y
) noexcept {
	for (const auto &unit : tick.units) {
		if (unit.x == x && unit.y == y) {
			return &unit;
		}
	}
	return nullptr;
}

const ReplayBullet *findBulletAt(
	const ReplayTickFrame &tick, int x, int y
) noexcept {
	for (const auto &bullet : tick.bullets) {
		if (bullet.x == x && bullet.y == y) {
			return &bullet;
		}
	}
	return nullptr;
}

MapCellVisual describeMapCell(
	const PlaybackState &playback_state, int x, int y
) noexcept {
	if (!playback_state.has_header) {
		return {
			.glyph = UiConst::glyph_empty,
			.fg = UiConst::color_plain,
			.emphasize = false,
		};
	}

	if (!isInBounds(playback_state.header.tilemap, x, y)) {
		return {
			.glyph = UiConst::glyph_empty,
			.fg = UiConst::color_plain,
			.emphasize = false,
		};
	}

	if (playback_state.has_tick) {
		if (const auto *unit = findUnitAt(playback_state.current_tick, x, y);
		    unit != nullptr) {
			return {
				.glyph = unit->player_id == 1 ? UiConst::glyph_player_1_unit
											  : UiConst::glyph_player_2_unit,
				.fg = UiConst::color_unit,
				.emphasize = true,
			};
		}

		if (const auto *bullet = findBulletAt(
				playback_state.current_tick, x, y
			);
		    bullet != nullptr) {
			return {
				.glyph = UiConst::glyph_bullet,
				.fg = bullet->player_id == 1 ? UiConst::color_bullet_p1
											 : UiConst::color_bullet_p2,
				.emphasize = true,
			};
		}
	}

	const auto *tile = tileAt(playback_state.header.tilemap, x, y);
	if (tile == nullptr) {
		return {
			.glyph = UiConst::glyph_empty,
			.fg = UiConst::color_plain,
			.emphasize = false,
		};
	}

	if (tile->is_base) {
		return {
			.glyph = UiConst::glyph_base,
			.fg = tile->side == 1 ? UiConst::color_base_p1
								  : UiConst::color_base_p2,
			.emphasize = false,
		};
	}

	if (tile->is_resource) {
		return {
			.glyph = UiConst::glyph_resource,
			.fg = UiConst::color_resource,
			.emphasize = false,
		};
	}

	if (tile->terrain == Tile::OBSTACLE) {
		return {
			.glyph = UiConst::glyph_obstacle,
			.fg = UiConst::color_obstacle,
			.emphasize = false,
		};
	}

	if (tile->terrain == Tile::WATER) {
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

void clampCamera(
	const PlaybackState &playback_state, CameraState &camera
) noexcept {
	if (!playback_state.has_header) {
		camera.x = 0;
		camera.y = 0;
		camera.viewport_size = 1;
		return;
	}

	const auto &tilemap = playback_state.header.tilemap;
	const int max_x = std::max(
		0, static_cast<int>(tilemap.width) - camera.viewport_size
	);
	const int max_y = std::max(
		0, static_cast<int>(tilemap.height) - camera.viewport_size
	);
	camera.x = std::clamp(camera.x, 0, max_x);
	camera.y = std::clamp(camera.y, 0, max_y);
}

int computeViewportSize(const PlaybackState &playback_state) noexcept {
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

	const int map_content_max_cols = std::max(1, (map_outer_max_width - 1) / 2);

	if (!playback_state.has_header) {
		return 1;
	}

	const auto &tilemap = playback_state.header.tilemap;
	return std::max(
		1,
		std::min(
			{static_cast<int>(tilemap.width), static_cast<int>(tilemap.height),
	         map_content_max_rows, map_content_max_cols}
		)
	);
}

void appendRenderedLogLines(
	const ReplayLogEntry &entry, std::vector<RenderedLogLine> &out_lines
) {
	auto payload_lines = formatReplayLogEntryLines(entry);
	for (auto it = payload_lines.rbegin(); it != payload_lines.rend(); ++it) {
		out_lines.push_back({*it, true});
	}
	out_lines.back().is_continuation = false;
}

std::vector<RenderedLogLine> collectVisibleLogLines(
	const PlaybackState &playback_state, int available_rows
) {
	if (available_rows <= 0) {
		return {};
	}

	std::vector<RenderedLogLine> lines;
	for (auto &entry : (playback_state.log_history | std::views::reverse)) {
		appendRenderedLogLines(entry, lines);
		if (lines.size() >= static_cast<std::size_t>(available_rows)) {
			break;
		}
	}

	if (lines.empty()) {
		lines.push_back({std::string(UiConst::no_logs_yet), false});
		return lines;
	}

	if (lines.size() > static_cast<std::size_t>(available_rows)) {
		lines.resize(available_rows);
	}
	std::reverse(lines.begin(), lines.end());
	return lines;
}

InfoPanelState buildInfoPanelState(
	const PlaybackState &playback_state, const CameraState &camera,
	std::chrono::milliseconds step_interval
) noexcept {
	InfoPanelState info_state;
	info_state.camera_x = camera.x;
	info_state.camera_y = camera.y;
	info_state.step_interval_ms = static_cast<int>(step_interval.count());
	info_state.stream_ended = playback_state.stream_ended;

	if (!playback_state.has_tick) {
		return info_state;
	}

	info_state.tick = playback_state.current_tick.tick;
	for (const auto &player : playback_state.current_tick.players) {
		if (player.base_capture_counter < 8) {
			continue;
		}
		info_state.game_over = true;
		info_state.captured_player_id = player.id;
		info_state.winner_player_id = static_cast<std::uint8_t>(3 - player.id);
		break;
	}

	return info_state;
}

ftxui::Element renderGameStatusLine(const InfoPanelState &info_state) {
	using namespace ftxui;

	if (info_state.game_over) {
		Element
			winner = text(std::format("P{} wins", info_state.winner_player_id))
			| color(UiConst::color_win) | bold;
		Element loser = text(
							std::format(
								"P{} base captured",
								info_state.captured_player_id
							)
						)
			| color(UiConst::color_loss) | bold;
		return hbox({text("Game over: "), winner, text(" | "), loser});
	}

	if (info_state.stream_ended) {
		return text("Replay stream ended");
	}
	return text("Replay stream running");
}

ftxui::Element renderLogPanel(
	const PlaybackState &playback_state, LogPanelLayoutState &layout_state
) {
	using namespace ftxui;

	auto visible_lines = collectVisibleLogLines(
		playback_state, layout_state.availableRows()
	);

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
		| reflect(layout_state.content_box);
	return window(text("Runtime logs"), std::move(content));
}

ftxui::Element renderInfoPanel(
	const PlaybackState &playback_state, const CameraState &camera,
	std::chrono::milliseconds step_interval, LogPanelLayoutState &layout_state
) {
	using namespace ftxui;
	const InfoPanelState info_state = buildInfoPanelState(
		playback_state, camera, step_interval
	);

	Element status_panel = window(
		text("Info"),
		vbox({
			text(std::string(UiConst::controls_line_1)),
			text(std::string(UiConst::controls_line_2)),
			separator(),
			text(std::format("Current tick: {}", info_state.tick)),
			renderGameStatusLine(info_state),
			text(
				std::format("Step interval: {} ms", info_state.step_interval_ms)
			),
			text(
				std::format(
					"View origin: ({}, {})", info_state.camera_x,
					info_state.camera_y
				)
			),
		})
	);

	return vbox(
		{status_panel, renderLogPanel(playback_state, layout_state) | flex}
	);
}

ftxui::Element renderMapCell(
	const PlaybackState &playback_state, int x, int y
) {
	using namespace ftxui;
	const MapCellVisual cell_visual = describeMapCell(playback_state, x, y);

	Element cell = text(std::string(1, cell_visual.glyph))
		| color(cell_visual.fg);
	if (cell_visual.emphasize) {
		cell = cell | bold;
	}
	return cell;
}

ftxui::Element renderMapPanel(
	const PlaybackState &playback_state, const CameraState &camera
) {
	using namespace ftxui;

	if (!playback_state.has_header) {
		return window(
			text("Map"), vbox({text(std::string(UiConst::waiting_for_stream))})
		);
	}

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
				renderMapCell(playback_state, camera.x + dx, camera.y + dy)
			);
		}
		rows.push_back(hbox(std::move(columns)));
	}

	const auto current_tick = playback_state.has_tick
		? playback_state.current_tick.tick
		: 0;
	const auto title = text(
		std::format(
			UiConst::map_title_format, playback_state.header.tilemap.width,
			playback_state.header.tilemap.height, current_tick
		)
	);

	Element body = vbox(std::move(rows))
		| size(WIDTH, EQUAL, camera.viewport_size * 2 - 1)
		| size(HEIGHT, EQUAL, camera.viewport_size);
	return window(title, body);
}

} // namespace

int runTui(
	ReplayByteStream &replay_stream, std::chrono::milliseconds step_interval
) {
	using namespace ftxui;

	CameraState camera;
	LogPanelLayoutState log_layout_state;
	PlaybackState playback_state;
	std::mutex playback_mutex;
	auto screen = ScreenInteractive::Fullscreen();

	std::jthread decode_thread([&](std::stop_token stop_token) {
		ReplayStreamDecoder decoder;
		while (!stop_token.stop_requested()) {
			std::vector<std::byte> chunk;
			const bool has_chunk = replay_stream.waitPopNext(
				chunk, std::chrono::milliseconds(50)
			);
			if (has_chunk) {
				decoder.pushBytes(chunk);
			}

			bool changed = false;

			std::optional<ReplayHeader> maybe_header;
			std::vector<ReplayTickFrame> new_ticks;
			while (true) {
				auto tick = decoder.tryReadNextTick();
				if (!tick.has_value()) {
					break;
				}
				new_ticks.push_back(std::move(*tick));
			}

			if (decoder.hasHeader()) {
				maybe_header = decoder.header();
			}

			{
				std::scoped_lock lock(playback_mutex);
				if (maybe_header.has_value() && !playback_state.has_header) {
					playback_state.header = std::move(*maybe_header);
					playback_state.has_header = true;
					changed = true;
				}

				for (auto &tick : new_ticks) {
					for (auto &log_entry : tick.logs) {
						playback_state.log_history.push_back(
							std::move(log_entry)
						);
					}
					playback_state.current_tick = std::move(tick);
					playback_state.has_tick = true;
					changed = true;
				}

				if (decoder.isEnded() && !playback_state.stream_ended) {
					playback_state.stream_ended = true;
					changed = true;
				}
			}

			if (changed) {
				screen.PostEvent(Event::Custom);
			}

			if (decoder.isEnded() && replay_stream.isDrained()) {
				break;
			}

			if (!has_chunk && replay_stream.isDrained()) {
				break;
			}
		}

		screen.PostEvent(Event::Custom);
	});

	auto root = Renderer([&] {
		std::scoped_lock lock(playback_mutex);
		camera.viewport_size = computeViewportSize(playback_state);
		clampCamera(playback_state, camera);

		Element map_panel = renderMapPanel(playback_state, camera);
		Element info_panel = renderInfoPanel(
			playback_state, camera, step_interval, log_layout_state
		);
		return hbox({map_panel, info_panel | flex});
	});

	root |= CatchEvent([&](Event event) {
		bool moved = false;
		if (event == Event::Custom) {
			return true;
		}

		if (event == Event::q || event == Event::Q || event == Event::Escape) {
			decode_thread.request_stop();
			screen.ExitLoopClosure()();
			return true;
		}

		if (event == Event::w || event == Event::W || event == Event::ArrowUp) {
			std::scoped_lock lock(playback_mutex);
			camera.y -= 1;
			clampCamera(playback_state, camera);
			moved = true;
		}
		if (event == Event::s || event == Event::S
		    || event == Event::ArrowDown) {
			std::scoped_lock lock(playback_mutex);
			camera.y += 1;
			clampCamera(playback_state, camera);
			moved = true;
		}
		if (event == Event::a || event == Event::A
		    || event == Event::ArrowLeft) {
			std::scoped_lock lock(playback_mutex);
			camera.x -= 1;
			clampCamera(playback_state, camera);
			moved = true;
		}
		if (event == Event::d || event == Event::D
		    || event == Event::ArrowRight) {
			std::scoped_lock lock(playback_mutex);
			camera.x += 1;
			clampCamera(playback_state, camera);
			moved = true;
		}
		return moved;
	});

	screen.Loop(root);
	decode_thread.request_stop();
	return 0;
}

} // namespace cr
