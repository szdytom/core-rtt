# Execution Architecture

This document describes the current `corertt` executable flow in `src/main.cpp`,
including thread interaction in live mode and replay playback mode.

## Entry flow

`main()` executes the following steps:

1. Parse CLI options (`parseOptions`).
2. Validate `--step-interval-ms > 0`.
3. Select mode:
   - `--play-replay <file>`: playback mode (`runPlaybackMode`).
   - otherwise: live simulation mode (`runLiveMode`).

All top-level exceptions are converted into stderr messages and process exit code
`1`.

## Shared runtime contract

Both modes use the same UI synchronization object:

- `SynchronizedReplayProgress replay_sync`
  - shared between producer thread and TUI thread
  - guarded by `replay_sync.mutex`
  - carries `ReplayProgress` and optional `last_error`

`TuiRunner` reads only from `replay_sync`, never from simulation internals.

## Live mode (`runLiveMode`)

### Initialization

1. Build `TilemapGenerationConfig` from CLI values.
2. Build `World` from generated tilemap.
3. Load player ELF binaries and install them into `World`.
4. Optionally open replay output file (`--replay-file`).
5. Build and publish replay header:
   - `ReplayHeader::fromWorld(world)`
   - write to `replay_sync.progress`
   - optional binary write to replay file

### Thread model

- Main thread:
  - runs `tui_runner.run()`.
- Producer thread (`std::jthread`):
  - loops at `step_interval_ms`
  - executes `world.step()`
  - captures tick snapshot (`ReplayTickFrame::fromWorldState(world)`)
  - publishes latest tick to `replay_sync`
  - optionally writes encoded tick bytes to replay file
  - exits on game over or stop request
  - emits and publishes end marker (`ReplayEndMarker::fromWorld(world)`)

`TuiRunner::notifyUpdate()` posts a custom FTXUI event to wake rendering when
new data arrives.

### Lifetime and ownership

- `World` is owned by `runLiveMode` stack frame and referenced by producer
  thread.
- Optional replay file stream is also owned by `runLiveMode` stack frame.
- `std::jthread` is destroyed before these objects during function exit, so all
  producer-thread references remain valid.

## Playback mode (`runPlaybackMode`)

### Initialization

1. Open replay file from `--play-replay`.
2. Create `ReplayStreamDecoder` with file stream adapter (`IstreamAdapter`).
3. Decode header once via `canReadHeader()` + `readHeader()`.
3. Start producer thread.

### Producer behavior

- Uses decoder readiness probes (`canReadNextRecord()`) before decode calls.
- Calls `nextTick()` only when a full record is available; this is a fail-fast
  contract and avoids partial-read branches.
- Publishes decoded tick/end progress into `replay_sync`.
- Reports missing end marker as decode error when stream reaches EOF before
  terminal record.
- Sleeps according to `step_interval_ms` between updates.

### UI behavior

- `TuiRunner` consumes `replay_sync` snapshots on custom events.
- Maintains local `PlaybackState` and log history for rendering.

## Shutdown sequence

For both modes:

1. `tui_runner.run()` returns when user quits UI.
2. Main thread calls `producer_thread.request_stop()`.
3. Function returns.
4. `std::jthread` destructor joins producer thread.

This guarantees producer thread completion before mode-local objects are
destroyed.

## Error handling

- CLI parse errors: printed together with help text.
- Mode runtime errors: printed in `main` catch block and return `1`.
- Producer-thread exceptions are caught and printed to stderr, then UI gets a
  final update event.

## Build targets

- `corertt_core`: simulation and replay library.
- `corertt`: executable for live mode and playback mode.
- `corertt_headless`: executable for single-thread live simulation replay generation without TUI.
- `corertt_replay_log`: replay log extraction tool (`text` / `jsonl`).

## Headless mode (`corertt_headless`)

`corertt_headless` runs a live simulation in a single thread and writes replay
output only.

Execution flow:

1. Parse CLI options.
2. Validate headless-only constraints:
  - `--play-replay` is not available in headless mode.
  - `--replay-file` is required.
3. Build `World` from map generation/file plus player ELF binaries.
4. Write replay header.
5. Loop until `world.gameOver()`:
  - `world.step()`
  - encode and write one `ReplayTickFrame`
6. Write replay end marker and exit.

Unlike `corertt`, headless mode has no producer/UI split, no `TuiRunner`, and
ignores `--step-interval-ms` so simulation runs at maximum speed.
