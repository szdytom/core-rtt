# Core RTT Autotesting

## Goal

The autotest pipeline validates integration across three layers:

- guest test program behavior (`corelib` C `.c` -> `.elf`, `rusty-corelib` Rust example -> ELF)
- `corertt_headless` engine execution and replay output (streamed on `stdout`)
- `@corertt/corereplay` stream decoding and assertion checks

`@corertt/core-autotest` does not require temporary replay files now. It runs
`corertt_headless` with `stdout` piped and decodes replay chunks incrementally
via `decodeReplayStream`.

## Workflow for New Cases

_Refer to the later sections for detailed explanations of each step._

1. **Write a guest program** for C cases, add source under `corelib/autotest`; for Rust cases, add an example under `rusty-corelib/examples`. The program should produce observable behavior (for example logs) that can be asserted.
2. **Create a JSON case file** for C under `corelib/autotest` or for Rust under `rusty-corelib/autotest`. The JSON defines execution bindings and assertions.
3. **Rebuild guest programs** C uses `make` in `corelib`; Rust uses `cargo build --release --examples` in `rusty-corelib`.
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

By default, the runner discovers all `*.json` files under both:

- `corelib/autotest`
- `rusty-corelib/autotest`

`--autotest-dir` is repeatable, so you can override with one or many directories.

You can override directory and binary path:

```bash
pnpm -C js --filter='@corertt/core-autotest' exec node dist/main.js \
  --autotest-dir /path/to/cases \
  --autotest-dir /path/to/another/cases \
  --headless /path/to/corertt_headless
```

## Program Reference Resolution

`program.base` and `program.unit` support these forms:

- `@alias`: resolve from default aliases + case-local aliases.
- explicit path (`.elf` suffix or contains `/`): resolve as absolute/relative path.
- namespaced symbol:
  1. `c::<name>` resolves within `corelib/autotest|test|example`.
  2. `rusty::<name>` resolves from `cargo build --release --examples --message-format=json` artifacts.

Notes:

- Symbol references must use `c::` or `rusty::` prefixes (aliases/paths are exceptions).
- Resolver checks all candidates and throws on ambiguity if multiple files match a single `c::<name>`.

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
- `seed`: forwarded to `--seed` (cannot be used with `map`)
- `map`: map filename loaded from `corelib/autotest/map` and forwarded to `--map` (cannot be used with `seed`)
- `aliases`: local alias map for `@alias` references
- `gameRules`: optional overrides forwarded to `corertt_headless` CLI flags. Supported fields are `width`, `height`, `baseSize`, `unitHealth`, `naturalEnergyRate`, `resourceZoneEnergyRate`, `attackCooldown`, `captureTurnThreshold`, `capacityLv1`, `capacityLv2`, `visionLv1`, `visionLv2`, `capacityUpgradeCost`, `visionUpgradeCost`, `damageUpgradeCost`, and `manufactCost`
- `expectedLogs`: assertions that require at least N matches
- `forbiddenLogs`: assertions that require at most N matches (default 0)
- `expectFailure`: if `true`, a run is considered pass only when assertions/runtime produce failure

`map` path rules:

- only a filename is accepted (no absolute path and no `/` or `\\` separators)
- runtime resolves to `<repo>/corelib/autotest/map/<filename>`
- case loading fails early if `seed` and `map` are both present

`forbiddenLogs` and `expectedLogs` item format:

- `filter.source`: `System`/`Player` (also supports enum integer)
- `filter.type`: `Custom`/`UnitCreation`/`UnitDestruction`/`ExecutionException`/`BaseCaptured` (also supports enum integer)
- `filter.playerId`: exact player id match
- `filter.unitId`: exact unit id match
- `filter.payload`: exact string or `{ "equals": "..." }` or `{ "regex": "..." }`

`gameRules` values are translated directly into `corertt_headless` arguments:

- `width` -> `--width`
- `height` -> `--height`
- `baseSize` -> `--base-size`
- `unitHealth` -> `--unit-health`
- `naturalEnergyRate` -> `--natural-energy-rate`
- `resourceZoneEnergyRate` -> `--resource-zone-energy-rate`
- `attackCooldown` -> `--attack-cooldown`
- `captureTurnThreshold` -> `--capture-turn-threshold`
- `capacityLv1` -> `--capacity-lv1`
- `capacityLv2` -> `--capacity-lv2`
- `visionLv1` -> `--vision-lv1`
- `visionLv2` -> `--vision-lv2`
- `capacityUpgradeCost` -> `--capacity-upgrade-cost`
- `visionUpgradeCost` -> `--vision-upgrade-cost`
- `damageUpgradeCost` -> `--damage-upgrade-cost`
- `manufactCost` -> `--manufact-cost`

## Positive Example

File: `corelib/autotest/log_quota.json`

This case validates log quota behavior and forbids any `System + ExecutionException + ABORTED` log.

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

Rust has an equivalent counterexample:

- `rusty-corelib/examples/abort_immediately.rs`
- `rusty-corelib/autotest/forbidden_log_counterexample.json`

Run it explicitly:

```bash
pnpm -C js --filter='@corertt/core-autotest' exec node dist/main.js \
  --case "rust forbiddenLogs interception counterexample"
```

There is also a positive Rust log case:

- `rusty-corelib/examples/log_simple.rs`
- `rusty-corelib/autotest/log_simple.json`
