# worker-codec

Binary protocol codec for communication between core-rtt backend and worker processes.

## Features

- Typed packet models for task assignment, task acknowledgment, and task result.
- Shared packet header with protocol/version validation.
- `encodePacket` and `decodePacket` helpers built on top of `structpack`.
- Explicit message type and result/status enums.

## Install

```sh
pnpm add @corertt/worker-codec
```

## Usage

```ts
import {
  decodePacket,
  encodePacket,
  MatchResult,
  StrategyExecutionCrashInfo,
  TaskResultPacket,
  TaskStatus,
} from '@corertt/worker-codec';

const crashA = new StrategyExecutionCrashInfo();
crashA.strategyGroupId = 1;
crashA.baseStrategyCrashed = false;
crashA.unitStrategyCrashed = false;

const crashB = new StrategyExecutionCrashInfo();
crashB.strategyGroupId = 2;
crashB.baseStrategyCrashed = true;
crashB.unitStrategyCrashed = false;

const packet = new TaskResultPacket();
packet.matchId = 42;
packet.result = MatchResult.P1Win;
packet.status = TaskStatus.Success;
packet.errorLog = '';
packet.crashInfo = [crashA, crashB];
packet.finishedAt = new Date();

const wire = encodePacket(packet);
const decoded = decodePacket(wire);
console.log(decoded);
```

## API

- `encodePacket(packet) => ArrayBuffer`
- `decodePacket(buffer) => Packet`
- `winnerIndexOf(result) => number | null`

## Development

```sh
pnpm run build
pnpm run typecheck
pnpm run test
```
