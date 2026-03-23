# Execution Architecture

This document describes runtime execution, replay production/consumption, and TUI interaction.

## Design goals

- Keep core simulation (`World`) independent from replay/UI ownership.
- Use replay stream as the single rendering data source.
- Support both live simulation and offline playback through the same decode path.
- Keep lock scope small by avoiding direct world reads from TUI.

## Main components

- `World`: game simulation state and `step()` progression.
- `ReplayTickFrame::fromWorldState`: snapshots world state after each tick.
- Replay static encoders (`ReplayHeader::encode`, `ReplayTickFrame::encode`, `ReplayEndMarker::encode`): convert structures to binary chunks.
- `ReplayByteStream`: thread-safe byte-chunk queue between producer and TUI consumer.
- `ReplayStreamDecoder`: incremental decoder used directly by UI thread.
- TUI `PlaybackState`: pure data snapshot used for rendering (header/current tick/log history).

## Live mode flow

```text
producer thread:
  world.step()
  tick = ReplayTickFrame::fromWorldState(world)
  ReplayTickFrame::encode(tick)
  replay_byte_stream.pushBytes(...)

UI thread:
  replay_byte_stream.waitPopNext(...)
  decoder.pushBytes(...)
  on Event::Custom: parse at most one tick
  update PlaybackState
  render from PlaybackState only
```

### Key properties

- TUI does not dereference `World`.
- Runtime logs shown in TUI are accumulated from replay tick logs.
- On shutdown (including user pressing `q`), producer emits replay end marker before closing stream.

## Playback mode flow

```text
producer thread:
  stream raw replay bytes into ReplayByteStream

TUI decode/render:
  identical to live mode, but tick pacing is controlled by UI thread
```

This keeps live and playback on a single decode path while moving playback pacing to UI control.

## Log lifecycle

- `World` stores per-tick logs only.
- At each tick capture, `ReplayTickFrame::fromWorldState` moves logs from world (`takeTickLogs()`) into replay tick frame.
- TUI appends each decoded tick's logs into its own `log_history` for scrolling/history display.

## Threading model

- Producer thread: generates replay byte chunks.
- UI event/render thread: consumes chunks, decodes records, and renders from playback state.

Shared object with synchronization:

- `ReplayByteStream`: internal mutex + condition variable.

The UI thread owns `PlaybackState`, so no additional playback-state mutex is needed.

## Failure handling

- Incremental decoder returns explicit read status (`NeedMoreData`, `Tick`, `End`) instead of using exceptions for incomplete input.
- Binary format v4 is size-framed (`header size`, tick `size`):
  - Declared size larger than known fields: decode known fields and ignore extension bytes.
  - Declared size smaller than known fields: treat as format error.
- Offline reader (`readReplay`) requires an end marker and rejects truncated files.
- Producer catches exceptions, reports to stderr, and closes stream to unblock TUI.

## Build targets

- `corertt_core`: reusable static library (simulation + replay logic).
- `corertt`: main executable (live mode + playback mode).
- `corertt_replay_log`: replay log extraction tool (`text` / `jsonl`).
