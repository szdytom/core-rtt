# Replay Binary Format

This document describes the on-disk and stream format produced by `ReplayStreamEncoder` and consumed by `ReplayStreamDecoder`.

## Byte order

All integer fields are little-endian.

## Top-level layout

A replay stream is:

1. Header
2. Zero or more tick records
3. End marker

```text
+-------------------+
| Header            |
+-------------------+
| Tick record #1    |
+-------------------+
| Tick record #2    |
+-------------------+
| ...               |
+-------------------+
| End marker        |
+-------------------+
```

## Header

### Magic and version

| Field | Type | Size | Notes |
|---|---|---:|---|
| magic | bytes | 4 | ASCII `CRPL` |
| version | u16 | 2 | Current value: `2` |
| reserved | u16 | 2 | Reserved for future use, currently `0` |

### Tilemap

| Field | Type | Size | Notes |
|---|---|---:|---|
| width | u16 | 2 | Map width |
| height | u16 | 2 | Map height |
| base_size | u16 | 2 | Base side length |
| reserved | u16 | 2 | Reserved, currently `0` |
| tiles | packed u8[] | width*height | Row-major, one byte per tile |

Packed tile bits (lowest bits first):

- bits `[0..1]`: `terrain`
- bits `[2..3]`: `side`
- bit `4`: `is_resource`
- bit `5`: `is_base`
- bits `[6..7]`: reserved

## Record framing

Each record starts with a one-byte record type.

| Record type | Value |
|---|---:|
| Tick | `1` |
| End | `255` |

## Tick record payload

After the `type=1` byte, payload fields are serialized in this exact order.

### Tick header

| Field | Type |
|---|---|
| tick | u32 |

### Players (fixed count = 2)

Each player item:

| Field | Type |
|---|---|
| id | u8 |
| base_energy | u16 |
| base_capture_counter | u8 |

### Units

| Field | Type | Notes |
|---|---|---|
| unit_count | u16 | Number of units in this tick |
| units | repeated | See unit item below |

Unit item:

| Field | Type |
|---|---|
| id | u8 |
| player_id | u8 |
| x | i16 |
| y | i16 |
| health | u8 |
| energy | u16 |
| attack_cooldown | u8 |
| upgrades | u8 |

### Bullets

| Field | Type | Notes |
|---|---|---|
| bullet_count | u16 | Number of bullets in this tick |
| bullets | repeated | See bullet item below |

Bullet item:

| Field | Type |
|---|---|
| x | i16 |
| y | i16 |
| direction | u8 |
| player_id | u8 |
| damage | u8 |

### Logs

| Field | Type | Notes |
|---|---|---|
| log_count | u16 | Number of logs in this tick |
| logs | repeated | See log item below |

Log item:

| Field | Type |
|---|---|
| tick | u32 |
| player_id | u8 |
| unit_id | u8 |
| source | u8 |
| type | u8 |
| payload_size | u16 |
| payload | bytes[payload_size] |

## End marker

After the `type=255` byte, payload fields are serialized in this order.

| Field | Type | Notes |
|---|---|---|
| termination | u8 | `0 = Completed`, `1 = Aborted` |
| winner_player_id | u8 | Winner player id (`1` or `2`) when `termination=Completed`, otherwise `0` |
| captured_player_id | u8 | Captured base owner (`1` or `2`) when `termination=Completed`, otherwise `0` |
| reserved | u8 | Reserved, currently `0` |

Semantics:

- `Completed`: The game ended by rules (base capture). `winner_player_id` and `captured_player_id` must be valid and complementary (`winner + captured = 3`).
- `Aborted`: Replay stream ended before the game reached a rule-defined winner (for example, user stopped live mode early). Winner and captured ids must both be `0`.

## Validation notes

A valid complete replay must have:

1. Correct magic and supported version.
2. A decodable header.
3. A terminating end marker with a valid payload.

The offline reader (`readReplay`) rejects streams without end marker.
