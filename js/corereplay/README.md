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
