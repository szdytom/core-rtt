# Core RTT Autotesting

## Goal

The autotest pipeline validates integration across three layers:

- `corelib` test program behavior (`.c` -> `.elf`)
- `corertt_headless` engine execution and replay output
- `@corertt/corereplay` replay decoding and assertion checks

## Workflow for New Cases

_Refer to the later sections for detailed explanations of each step._

1. **Write a C program** a new test case usually involves writing new guest C code under `corelib/autotest`. This program should implement the test logic and produce observable behavior (e.g. logs) that can be asserted against.
2. **Create a JSON case file** for the new program under `corelib/autotest`. This file defines how the runner should execute the test and what assertions to check on the replay logs.
3. **Rebuild the test program** invoke `make` in the `corelib` directory. The `Makefile` uses wildcard patterns to automatically discover all `*.c` files under `corelib/autotest` and build them into corresponding `.elf` binaries.
4. **Run the autotest runner** use `scripts/autotest.sh` to evaluate all cases involving the new case.

Hints for avoiding common pitfalls:

1. Properly escape regexes in JSON.

## Entry Command

TLDR: There is a convenient "scripts/autotest.sh" wrapper for the main runner command, it checks prerequisites and runs all cases.

Run from repository root:

```bash
pnpm -C js --filter='@corertt/core-autotest' exec node dist/main.js
```

Run one case by exact name:

```bash
pnpm -C js --filter='@corertt/core-autotest' exec node dist/main.js --case "ecall log quota test"
```

## Case File Discovery

By default, the runner discovers all `*.json` files under `corelib/autotest`.

You can override directory and binary path:

```bash
pnpm -C js --filter='@corertt/core-autotest' exec node dist/main.js \
  --autotest-dir /path/to/cases \
  --headless /path/to/corertt_headless
```

## Case Schema (v1)

Each case file is one JSON object.

Required fields:

- `version`: must be `1`
- `name`: human-readable case name
- `timeLimitMs`: per-run timeout in milliseconds
- `maxTicks`: `--max-ticks` passed to `corertt_headless`
- `program`: exactly two player entries (`[p1, p2]`), each with `base` and `unit`

Optional fields:

- `runs`: repeat count (default `1`)
- `seed`: forwarded to `--seed`
- `aliases`: local alias map for `@alias` references
- `expectedLogs`: assertions that require at least N matches
- `forbiddenLogs`: assertions that require at most N matches (default 0)
- `expectFailure`: if `true`, a run is considered pass only when assertions/runtime produce failure

`forbiddenLogs` and `expectedLogs` item format:

- `filter.source`: `System`/`Player` (also supports enum integer)
- `filter.type`: `Custom`/`UnitCreation`/`UnitDestruction`/`ExecutionException`/`BaseCaptured` (also supports enum integer)
- `filter.playerId`: exact player id match
- `filter.unitId`: exact unit id match
- `filter.payload`: exact string or `{ "equals": "..." }` or `{ "regex": "..." }`

## Positive Example

File: `corelib/autotest/log_quota.json`

This case validates log quota behavior and forbids any `System + ExecutionException + ABORTED` log.

## Migrated Assert Cases

The following legacy `corelib/test` programs are migrated to assert-driven autotests under `corelib/autotest`:

- `msg.c` + `msg.json`
- `heap.c` + `heap.json`
- `deposit_withdraw.c` + `deposit_withdraw.json`

Each migrated case follows the same pattern:

- program logic is assert-driven (wrong behavior triggers runtime abort)
- JSON contains a `forbiddenLogs` rule forbidding `System + ExecutionException + ABORTED`
- JSON contains an `expectedLogs` rule matching a `[PASS] ...` payload to ensure the test path is really executed

Run a migrated case by name:

```bash
pnpm -C js --filter='@corertt/core-autotest' exec node dist/main.js --case "heap ecall assert test"
```

## Counterexample For forbiddenLogs Interception

Files:

- `corelib/autotest/abort_immediately.c`
- `corelib/autotest/forbidden_log_counterexample.json`

`abort_immediately` calls `abort()` directly, which produces a replay log with:

- source = `System`
- type = `ExecutionException`
- payload = `ABORTED`

The JSON case forbids exactly that log and sets `expectFailure: true`.

This proves forbidden log matching is actively intercepting violations.

Run it explicitly:

```bash
pnpm -C js --filter='@corertt/core-autotest' exec node dist/main.js \
  --case "forbiddenLogs interception counterexample"
```

Expected result should include:

- `PASS ... (expected failure observed)`

If you remove `expectFailure: true`, the same case must fail, demonstrating the interceptor blocks the run as intended.
