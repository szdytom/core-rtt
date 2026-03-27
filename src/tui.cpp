#include "corertt/tui.h"
#include "corertt/fail_fast.h"
#include "corertt/replay.h"
#include "corertt/tilemap.h"
#include <algorithm>
#include <cstdint>
#include <format>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace cr {

namespace {

namespace UIConst {
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
const ftxui::Color color_terminated = ftxui::Color::RGB(237, 189, 86);
} // namespace UIConst

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
	ReplayProgress progress;
	std::vector<ReplayLogEntry> log_history;
	std::string last_error;
	std::string tilemap_source = "(unknown)";

	std::uint32_t currentTick() const noexcept {
		if (progress.phase == ReplayParsePhase::Header) {
			return 0;
		}
		return progress.current_tick.tick;
	}

	bool hasHeader() const noexcept {
		return progress.phase != ReplayParsePhase::Header;
	}
};

struct MapCellVisual {
	char glyph = UIConst::glyph_plain;
	ftxui::Color fg = UIConst::color_plain;
	bool emphasize = false;
};

bool isInBounds(const ReplayTilemap &tilemap, int x, int y) noexcept {
	return x >= 0 && y >= 0 && x < tilemap.width && y < tilemap.height;
}

TileFlags tileAt(const ReplayTilemap &tilemap, int x, int y) noexcept {
	return tilemap.tiles[y * tilemap.width + x];
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
	CR_FAIL_FAST_ASSERT_LIGHT(
		playback_state.hasHeader(), "replay header is not available"
	);

	const auto &tilemap = playback_state.progress.header.tilemap;
	const auto &current_tick = playback_state.progress.current_tick;

	CR_FAIL_FAST_ASSERT_LIGHT(
		isInBounds(tilemap, x, y),
		std::format("out-of-bounds coordinate ({}, {})", x, y)
	);

	const auto tile = tileAt(tilemap, x, y);

	if (const auto *unit = findUnitAt(current_tick, x, y); unit != nullptr) {
		return {
			.glyph = unit->player_id == 1 ? UIConst::glyph_player_1_unit
										  : UIConst::glyph_player_2_unit,
			.fg = UIConst::color_unit,
			.emphasize = true,
		};
	}

	if (const auto *bullet = findBulletAt(current_tick, x, y);
	    bullet != nullptr) {
		return {
			.glyph = UIConst::glyph_bullet,
			.fg = bullet->player_id == 1 ? UIConst::color_bullet_p1
										 : UIConst::color_bullet_p2,
			.emphasize = true,
		};
	}

	if (tile.is_base) {
		return {
			.glyph = UIConst::glyph_base,
			.fg = tile.side == 1 ? UIConst::color_base_p1
								 : UIConst::color_base_p2,
			.emphasize = false,
		};
	}

	if (tile.is_resource) {
		return {
			.glyph = UIConst::glyph_resource,
			.fg = UIConst::color_resource,
			.emphasize = false,
		};
	}

	if (tile.terrain == Tile::OBSTACLE) {
		return {
			.glyph = UIConst::glyph_obstacle,
			.fg = UIConst::color_obstacle,
			.emphasize = false,
		};
	}

	if (tile.terrain == Tile::WATER) {
		return {
			.glyph = UIConst::glyph_water,
			.fg = UIConst::color_water,
			.emphasize = false,
		};
	}

	return {
		.glyph = UIConst::glyph_plain,
		.fg = UIConst::color_plain,
		.emphasize = false,
	};
}

void clampCamera(
	const PlaybackState &playback_state, CameraState &camera
) noexcept {
	if (!playback_state.hasHeader()) {
		camera.x = 0;
		camera.y = 0;
		camera.viewport_size = 1;
		return;
	}

	const auto &tilemap = playback_state.progress.header.tilemap;
	const int max_x = std::max(0, tilemap.width - camera.viewport_size);
	const int max_y = std::max(0, tilemap.height - camera.viewport_size);
	camera.x = std::clamp(camera.x, 0, max_x);
	camera.y = std::clamp(camera.y, 0, max_y);
}

int computeViewportSize(const PlaybackState &playback_state) noexcept {
	const auto terminal_size = ftxui::Terminal::Size();
	const int terminal_cols = std::max(1, terminal_size.dimx);
	const int terminal_rows = std::max(1, terminal_size.dimy);

	const int map_content_max_rows = std::max(
		1, terminal_rows - (UIConst::min_map_window_outer_height - 1)
	);

	const int reserved_info_width = terminal_cols
			> UIConst::info_panel_preferred_min_width
		? UIConst::info_panel_preferred_min_width
		: 0;
	const int map_outer_max_width = std::max(
		UIConst::min_map_window_outer_width, terminal_cols - reserved_info_width
	);

	const int map_content_max_cols = std::max(1, (map_outer_max_width - 1) / 2);

	if (!playback_state.hasHeader()) {
		return 1;
	}

	const auto &tilemap = playback_state.progress.header.tilemap;
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
	const auto group_begin = out_lines.size();
	for (auto &line : FormatReplayLogEntryLines(entry)) {
		out_lines.push_back({std::move(line), true});
	}

	const auto group_begin_it = out_lines.begin() + group_begin;
	std::reverse(group_begin_it, out_lines.end());
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
		lines.push_back({std::string(UIConst::no_logs_yet), false});
		return lines;
	}

	if (lines.size() > static_cast<std::size_t>(available_rows)) {
		lines.resize(available_rows);
	}
	std::reverse(lines.begin(), lines.end());
	return lines;
}

ftxui::Element renderGameStatusLine(const PlaybackState &playback_state) {
	using namespace ftxui;

	if (playback_state.last_error.size() > 0) {
		return text(std::format("Decode error: {}", playback_state.last_error))
			| color(UIConst::color_loss) | bold;
	}

	if (playback_state.progress.phase == ReplayParsePhase::End) {
		const auto &end_marker = playback_state.progress.end_marker;
		if (end_marker.termination == ReplayTermination::Completed) {
			auto winner = text(
							  std::format(
								  "P{} wins", end_marker.winner_player_id
							  )
						  )
				| color(UIConst::color_win) | bold;
			return hbox({text("Game over: "), winner});
		}

		return text("Replay terminated early (unfinished)")
			| color(UIConst::color_terminated) | bold;
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
	LogPanelLayoutState &layout_state
) {
	using namespace ftxui;

	Element status_panel = window(
		text("Info"),
		vbox({
			text(std::string(UIConst::controls_line_1)),
			text(std::string(UIConst::controls_line_2)),
			separator(),
			text(std::format("Current tick: {}", playback_state.currentTick())),
			text(playback_state.tilemap_source),
			renderGameStatusLine(playback_state),
			text(
				std::format(
					"View origin: ({}, {}) - ({}, {})", camera.x, camera.y,
					camera.x + camera.viewport_size - 1,
					camera.y + camera.viewport_size - 1
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

	if (!playback_state.hasHeader()) {
		return window(
			text("Map"), vbox({text(std::string(UIConst::waiting_for_stream))})
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

	const auto title = text(
		std::format(
			UIConst::map_title_format,
			playback_state.progress.header.tilemap.width,
			playback_state.progress.header.tilemap.height,
			playback_state.currentTick()
		)
	);

	Element body = vbox(std::move(rows))
		| size(WIDTH, EQUAL, camera.viewport_size * 2 - 1)
		| size(HEIGHT, EQUAL, camera.viewport_size);
	return window(title, body);
}

} // namespace

TuiRunner::TuiRunner() noexcept
	: _screen(ftxui::ScreenInteractive::Fullscreen()) {}

TuiRunner::~TuiRunner() {
	requestStop();
	if (_ui_thread.joinable()) {
		_ui_thread.join();
	}
}

void TuiRunner::start() {
	if (_ui_thread.joinable()) {
		return;
	}
	_ui_thread = std::jthread([this](std::stop_token stop_token) {
		runUIThread(stop_token);
	});
}

int TuiRunner::wait() {
	if (_ui_thread.joinable()) {
		_ui_thread.join();
	}
	return 0;
}

void TuiRunner::publishHeader(const ReplayHeader &header) {
	{
		std::lock_guard lock(_replay.mutex);
		_replay.progress.phase = ReplayParsePhase::Tick;
		_replay.progress.header = header;
	}
	notifyUpdate();
}

void TuiRunner::publishTick(const ReplayTickFrame &tick) {
	{
		std::lock_guard lock(_replay.mutex);
		_replay.progress.current_tick = tick;
	}
	notifyUpdate();
}

void TuiRunner::publishEnd(const ReplayEndMarker &end_marker) {
	{
		std::lock_guard lock(_replay.mutex);
		_replay.progress.phase = ReplayParsePhase::End;
		_replay.progress.end_marker = end_marker;
	}
	notifyUpdate();
}

void TuiRunner::publishError(const std::string &message) {
	{
		std::lock_guard lock(_replay.mutex);
		_replay.last_error = message;
	}
	notifyUpdate();
}

void TuiRunner::publishTilemapSource(const std::string &source) {
	{
		std::lock_guard lock(_replay.mutex);
		_replay.tilemap_source = source;
	}
	notifyUpdate();
}

bool TuiRunner::shouldStop() const noexcept {
	return _ui_thread.joinable()
		&& _ui_thread.get_stop_token().stop_requested();
}

void TuiRunner::requestStop() noexcept {
	if (_ui_thread.joinable()) {
		_ui_thread.request_stop();
	}
	_screen.PostEvent(ftxui::Event::Custom);
}

void TuiRunner::notifyUpdate() noexcept {
	_screen.PostEvent(ftxui::Event::Custom);
}

void TuiRunner::runUIThread(std::stop_token stop_token) {
	using namespace ftxui;

	CameraState camera;
	LogPanelLayoutState log_layout_state;
	PlaybackState playback_state;

	auto root = Renderer([&] {
		camera.viewport_size = computeViewportSize(playback_state);
		clampCamera(playback_state, camera);

		Element map_panel = renderMapPanel(playback_state, camera);
		Element info_panel = renderInfoPanel(
			playback_state, camera, log_layout_state
		);
		return hbox({map_panel, info_panel | flex});
	});

	root |= CatchEvent([&](Event event) {
		bool moved = false;
		if (event == Event::Custom) {
			if (stop_token.stop_requested() || shouldStop()) {
				_screen.ExitLoopClosure()();
				return true;
			}

			std::lock_guard lock(_replay.mutex);
			auto prev_tick = playback_state.currentTick();
			if (_replay.progress.phase != playback_state.progress.phase) {
				playback_state.progress = _replay.progress;
			} else if (_replay.progress.phase != ReplayParsePhase::Header) {
				playback_state.progress
					.current_tick = _replay.progress.current_tick;
			}

			if (_replay.last_error.size() > 0) {
				playback_state.last_error = _replay.last_error;
			}

			if (_replay.tilemap_source.size() > 0) {
				playback_state.tilemap_source = _replay.tilemap_source;
			}

			// concat new log entries to history
			if (playback_state.progress.phase != ReplayParsePhase::Header
			    && playback_state.progress.current_tick.tick != prev_tick) {
				const auto &new_tick = playback_state.progress.current_tick;
				playback_state.log_history.insert(
					playback_state.log_history.end(), new_tick.logs.begin(),
					new_tick.logs.end()
				);

				constexpr std::size_t max_log_history = 120;
				if (playback_state.log_history.size() > 2 * max_log_history) {
					playback_state.log_history.erase(
						playback_state.log_history.begin(),
						playback_state.log_history.end() - max_log_history
					);
				}
			}
			return true;
		}

		if (event == Event::q || event == Event::Q || event == Event::Escape) {
			if (_ui_thread.joinable()) {
				_ui_thread.request_stop();
			}
			_screen.ExitLoopClosure()();
			return true;
		}

		if (event == Event::w || event == Event::W || event == Event::ArrowUp) {
			camera.y -= 1;
			clampCamera(playback_state, camera);
			moved = true;
		}
		if (event == Event::s || event == Event::S
		    || event == Event::ArrowDown) {
			camera.y += 1;
			clampCamera(playback_state, camera);
			moved = true;
		}
		if (event == Event::a || event == Event::A
		    || event == Event::ArrowLeft) {
			camera.x -= 1;
			clampCamera(playback_state, camera);
			moved = true;
		}
		if (event == Event::d || event == Event::D
		    || event == Event::ArrowRight) {
			camera.x += 1;
			clampCamera(playback_state, camera);
			moved = true;
		}
		return moved;
	});

	notifyUpdate();
	_screen.Loop(root);
}

} // namespace cr
