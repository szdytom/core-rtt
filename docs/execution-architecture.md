# Execution Architecture

This document describes the `corertt` executable flow in `src/main.cpp`,
including the UI abstraction layer and thread interaction in live/playback
modes.

## Entry flow

`main()` executes the following steps:

1. Parse CLI options (`parseOptions`).
2. Validate `--step-interval-ms > 0`.
3. Create UI instance from `--ui-mode`:
   - `tui`: `TuiUi`
   - `plain`: `PlainUi`
4. Select runtime mode:
   - `--play-replay <file>`: playback mode (`runPlaybackMode`).
   - otherwise: live simulation mode (`runLiveMode`).

All top-level exceptions are converted into stderr messages and process exit code
`1`.

## UI abstraction contract

`corertt` runtime drives UI only through `IUi` (`include/corertt/ui.h`):

```cpp
class IUi {
public:
    virtual ~IUi() = default;

    virtual void start() = 0;
    virtual int wait() = 0;

    virtual void publishHeader(const ReplayHeader& header) = 0;
    virtual void publishTick(const ReplayTickFrame& tick) = 0;
    virtual void publishEnd(const ReplayEndMarker& end_marker) = 0;
    virtual void publishError(const std::string& message) = 0;

    virtual bool shouldStop() const noexcept = 0;
    virtual void requestStop() noexcept = 0;
};
```

Method semantics:

- `start()`: initialize UI runtime. `TuiUi` starts an internal UI thread.
  `PlainUi` performs lightweight startup output.
- `publishHeader/publishTick/publishEnd/publishError`: runtime-to-UI event
  channel. Runtime never directly reads/writes UI internals.
- `shouldStop()`: checks whether user requested quit (for example Q/Esc in TUI).
- `requestStop()`: runtime-initiated shutdown (used on exception paths).
- `wait()`: blocks until UI finishes and returns UI exit code.

## Live mode (`runLiveMode`)

### Initialization

1. `ui.start()`.
2. Build `World` from CLI options.
3. Optionally open replay output file (`--replay-file`).
4. Build header (`ReplayHeader::fromWorld(world)`) and publish via
   `ui.publishHeader(...)`.
5. Optionally write binary header to replay file.

### Runtime behavior

Main thread runs the simulation loop:

- loop while `!ui.shouldStop()`
- `world.step()`
- capture tick (`ReplayTickFrame::fromWorldState(world)`)
- `ui.publishTick(...)`
- optional replay write
- stop on `world.gameOver()`
- throttle with `--step-interval-ms`

After loop:

- emit `ReplayEndMarker::fromWorld(world)`
- publish end marker via `ui.publishEnd(...)`
- write end marker if replay output is enabled
- `return ui.wait()`

## Playback mode (`runPlaybackMode`)

### Initialization

1. `ui.start()`.
2. Open replay file (`--play-replay`).
3. Create `ReplayStreamDecoder`.
4. Decode header once and publish via `ui.publishHeader(...)`.

### Runtime behavior

Main thread runs decoder loop:

- loop while `!ui.shouldStop()`
- detect stream end and publish end marker
- call `nextTick()` only when `canReadNextRecord()` is true
- publish tick on successful tick decode
- publish decode error on malformed stream
- throttle with `--step-interval-ms`

Finally `return ui.wait()`.

## UI mode threading model

### `--ui-mode=tui`

- Main thread: simulation/decode runtime loop.
- UI thread (`std::jthread` inside `TuiUi`): FTXUI event loop and rendering.
- Runtime publishes updates into `TuiUi` synchronized state
  (`TuiSynchronizedReplayProgress`), and `TuiUi` wakes renderer via custom
  events.

### `--ui-mode=plain`

- Single thread only.
- Runtime loop and text output run on main thread.
- No producer/UI thread split and no event queue.

## Shutdown and error handling

- User quit is reflected by `ui.shouldStop()`.
- Runtime exceptions trigger `ui.requestStop()` before rethrow.
- Main catch block prints error and returns `1`.
- CLI parse errors print help text and return `1`.

## Build targets

- `corertt_core`: simulation and replay library.
- `corertt`: executable for live mode and replay playback mode
  (`--ui-mode=tui|plain`).
- `corertt_headless`: single-thread live simulation replay generation without
  UI.
- `corertt_replay_log`: replay log extraction tool (`text` / `jsonl`).

## Headless mode (`corertt_headless`)

`corertt_headless` remains unchanged and runs live simulation in a single
thread with replay output only:

1. Parse CLI options.
2. Validate headless-only constraints.
3. Build `World`.
4. Write replay header.
5. Loop `world.step()` until `world.gameOver()` or `--max-ticks`.
6. Write replay end marker and exit.

Unlike `corertt`, headless mode ignores `--step-interval-ms` to maximize
throughput.
