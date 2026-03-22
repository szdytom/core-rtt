# Execution Architecture

This document describes runtime execution, replay production/consumption, and TUI interaction.

## Design goals

- Keep core simulation (`World`) independent from replay/UI ownership.
- Use replay stream as the single rendering data source.
- Support both live simulation and offline playback through the same decode path.
- Keep lock scope small by avoiding direct world reads from TUI.

## Main components

- `World`: game simulation state and `step()` progression.
- `ReplayRecorder`: snapshots world state after each tick.
- `ReplayStreamEncoder`: converts replay header/ticks/end marker to binary chunks.
- `ReplayByteStream`: thread-safe byte-chunk queue between producer and TUI consumer.
- `ReplayStreamDecoder`: incremental decoder used by TUI.
- TUI `PlaybackState`: pure data snapshot used for rendering (header/current tick/log history).

## Live mode flow

```text
producer thread:
  world.step()
  replay_recorder.addTick(world)
  replay_encoder.encodeTick(...)
  replay_byte_stream.pushBytes(...)

TUI decode thread:
  replay_byte_stream.waitPopNext(...)
  decoder.pushBytes(...)
  decoder.tryReadNextTick() loop
  update PlaybackState

UI thread:
  render from PlaybackState only
```

### Key properties

- TUI does not dereference `World`.
- Runtime logs shown in TUI are accumulated from replay tick logs.
- On shutdown (including user pressing `q`), producer emits replay end marker before closing stream.

## Playback mode flow

```text
producer thread:
  readReplay(file)
  re-encode header/ticks/end as stream chunks
  push chunks into ReplayByteStream with playback interval

TUI decode/render:
  identical to live mode
```

This makes live display a special case of replay playback.

## Log lifecycle

- `World` stores per-tick logs only.
- At each tick capture, `ReplayRecorder::addTick` moves logs from world (`takeTickLogs()`) into replay tick frame.
- TUI appends each decoded tick's logs into its own `log_history` for scrolling/history display.

## Threading model

- Producer thread: generates replay byte chunks.
- Decode thread (inside TUI): consumes chunks and updates playback state under mutex.
- UI event/render thread: reads playback state under the same mutex.

Shared object with synchronization:

- `ReplayByteStream`: internal mutex + condition variable.
- `PlaybackState`: guarded by TUI-local mutex.

## Failure handling

- Incremental decoder returns no tick when bytes are insufficient and waits for more data.
- Offline reader (`readReplay`) requires an end marker and rejects truncated files.
- Producer catches exceptions, reports to stderr, and closes stream to unblock TUI.

## Build targets

- `corertt_core`: reusable static library (simulation + replay logic).
- `corertt`: main executable (live mode + playback mode).
- `corertt_replay_log`: replay log extraction tool (`text` / `jsonl`).
