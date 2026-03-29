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
cp js/core-worker/.env.example js/core-worker/.env
# edit js/core-worker/.env

pnpm -C js --filter @corertt/core-worker run build
pnpm -C js --filter @corertt/core-worker exec node dist/main.js
```

## Environment keys

- `CORERTT_WORKER_BACKEND_URL` (required)
- `CORERTT_WORKER_ID` (required)
- `CORERTT_WORKER_SECRET` (required)
- `CORERTT_WORKER_HEADLESS_PATH`: default `<repo>/build/corertt_headless`
- `CORERTT_WORKER_TIMEOUT_SECONDS`: default `1800`
- `CORERTT_WORKER_MAX_TICKS`: default `5000`
- `CORERTT_WORKER_ELF_CACHE_DIR`: default `/dev/shm/corertt-elf-cache`
- `CORERTT_WORKER_ELF_CACHE_LIMIT`: default `200M`
- `CORERTT_WORKER_TMP_MAP_DIR`: default `/dev/shm/corertt-tmp-map`
- `CORERTT_WORKER_CONCURRENCY`: default `2`
- `CORERTT_WORKER_ERROR_LOG_MAX_BYTES`: default `65536`
- `CORERTT_WORKER_DOWNLOAD_USE_BEARER`: default `0`
- `CORERTT_WORKER_ENV_PATH`: optional explicit `.env` path

## Mock backend testing

A reusable mock backend is provided in `src/mock-server.ts` and covered by integration test `test/worker.e2e.spec.ts`.

Run tests:

```bash
pnpm -C js --filter @corertt/core-worker run test
```
