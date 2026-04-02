# Replay Binary Format

This document describes the on-disk and stream format produced by replay static encoders (`ReplayHeader::encode`, `ReplayTickFrame::encode`, `ReplayEndMarker::encode`) and consumed by `ReplayStreamDecoder`.

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
| version | u16 | 2 | Current value: `5` |
| header size | u16 | 2 | The size of the header payload in bytes |

The header size field is the size of the entire header after the header size field itself, i.e. the combined size of the tilemap and game rules for now.

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

### Game rules

The game rules payload follows the tilemap in the header body.

| Field | Type | Notes |
|---|---|---|
| width | u8 | Map width |
| height | u8 | Map height |
| base_size | u8 | Base side length |
| unit_health | u8 | Unit health |
| natural_energy_rate | u8 | Natural energy rate |
| resource_zone_energy_rate | u8 | Resource-zone energy rate |
| attack_cooldown | u8 | Attack cooldown |
| capture_turn_threshold | u8 | Capture turn threshold |
| vision_lv1 | u8 | Vision level 1 |
| vision_lv2 | u8 | Vision level 2 |
| capacity_lv1 | u16 | Capacity upgrade level 1 |
| capacity_lv2 | u16 | Capacity upgrade level 2 |
| capacity_upgrade_cost | u16 | Capacity upgrade cost |
| vision_upgrade_cost | u16 | Vision upgrade cost |
| damage_upgrade_cost | u16 | Damage upgrade cost |
| manufact_cost | u16 | Manufacturing cost |

### End of header

After the game rules, the header ends. If the header size field says there are extra bytes after the game rules, those bytes should be ignored and not treated as a format error.

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
| size | u32 | The size of the tick payload in bytes |

The size of the tick payload is the size of the entire tick record after the `size` field itself, i.e. the sum of the sizes of the Players, Units, Bullets, and Logs sections described below.

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
| x | u8 |
| y | u8 |
| health | u8 |
| attack_cooldown | u8 |
| upgrades | u8 |
| energy | u16 |

### Bullets

| Field | Type | Notes |
|---|---|---|
| bullet_count | u16 | Number of bullets in this tick |
| bullets | repeated | See bullet item below |

Bullet item:

| Field | Type |
|---|---|
| x | u8 |
| y | u8 |
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

### End of tick record

After the units, bullets, and logs sections, the tick record ends. If the tick record size field says there are extra bytes after the logs section, those bytes should be ignored and not treated as a format error.

## End marker

After the `type=255` byte, payload fields are serialized in this order.

| Field | Type | Notes |
|---|---|---|
| termination | u8 | `0 = Completed`, `1 = Aborted` |
| winner_player_id | u8 | Winner player id (`1` or `2`) when `termination=Completed`, otherwise `0` |

Semantics:

- `Completed`: The game ended by rules (base capture).
- `Aborted`: Replay stream ended before the game reached a rule-defined winner (for example, user stopped live mode early). Winner id should be `0` in this case.

## Validation notes

A valid complete replay must have:

1. Correct magic and supported version.
2. A decodable header.
3. A terminating end marker with a valid payload.

The offline reader (`readReplay`) rejects streams without end marker.
