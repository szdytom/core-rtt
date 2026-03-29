# @corertt/core-worker

A TypeScript worker implementation for Core RTT backend tasks.

## What it does

- Authenticates with backend by `POST /api/worker/authenticate`.
- Connects to `WebSocket /api/worker/communication` with bearer token.
- Receives `TaskAssignPacket`, sends `TaskAckownledgedPacket`, and executes tasks concurrently.
- Downloads strategy ELF files and stores them in an LRU cache.
- Runs `corertt_headless` with timeout and optional map temporary file.
- Captures `stdout` and `stderr`.
- Compresses replay bytes with zstd (native module preferred, `zstd` CLI fallback).
- Uploads compressed replay by direct `PUT` to backend URL.
- Sends `TaskResultPacket` back with `errorLog` truncation.

## CLI usage

```bash
pnpm -C js --filter @corertt/core-worker run build
pnpm -C js --filter @corertt/core-worker exec node dist/main.js \
  --backend-url http://127.0.0.1:3000 \
  --worker-id worker-1 \
  --worker-secret secret-1
```

## Key options

- `--headless-path`: default `<repo>/build/corertt_headless`
- `--timeout-seconds`: default `1800`
- `--max-ticks`: default `5000`
- `--elf-cache-dir`: default `/dev/shm/corertt-elf-cache`
- `--elf-cache-limit`: default `200M`
- `--tmp-map-dir`: default `/dev/shm/corertt-tmp-map`
- `--concurrency`: default `2`
- `--error-log-max-bytes`: default `65536`
- `--download-use-bearer`: disabled by default

## Mock backend testing

A reusable mock backend is provided in `src/mock-server.ts` and covered by integration test `test/worker.e2e.spec.ts`.

Run tests:

```bash
pnpm -C js --filter @corertt/core-worker run test
```
