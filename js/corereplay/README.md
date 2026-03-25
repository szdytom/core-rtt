# corereplay

TypeScript decoder for the core-rtt replay binary format.

## Features

- Decode full replay bytes with `decodeReplay`.
- Decode async streams with `decodeReplayStream`.
- Supports both `AsyncIterable` and `ReadableStream` input.
- Strict validation by default.
- Log payloads are exposed as raw `Uint8Array` only.

## Usage

```ts
import { decodeReplay } from '@corertt/corereplay';

const replay = decodeReplay(bytes);
console.log(replay.header.version);
console.log(replay.ticks.length);
```

### Strict vs non-strict decoding

Decoding is strict by default. This requires a complete replay stream with a valid end marker.

Set `strict: false` to enable tolerant decoding for incomplete files (for example: header-only files, missing end marker, or a truncated final tick record). In this mode:

- all complete tick frames are returned;
- trailing incomplete bytes are ignored;
- an aborted end marker is synthesized automatically.

```ts
import { decodeReplay } from '@corertt/corereplay';

const replay = decodeReplay(bytes, { strict: false });
console.log(replay.endMarker); // { termination: 'aborted', winnerPlayerId: 0 }
```

```ts
import { decodeReplayStream } from '@corertt/corereplay';

for await (const event of decodeReplayStream(stream)) {
  if (event.kind === 'header') {
    console.log('header', event.header);
  } else if (event.kind === 'tick') {
    console.log('tick', event.tick.tick);
  } else {
    console.log('end', event.endMarker);
  }
}
```

## API

- `decodeReplay(input, options?) => ReplayData`
- `decodeReplayStream(input, options?) => AsyncGenerator<ReplayStreamEvent>`
- `decodeReplayFromStream(input, options?) => Promise<ReplayData>`
