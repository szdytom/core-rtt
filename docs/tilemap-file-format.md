# Tilemap File Format

This document describes the on-disk formats for tilemap files, as produced and consumed by `Tilemap::saveAsText`, `Tilemap::saveAsBinary`, `Tilemap::loadText`, `Tilemap::loadBinary`, and `Tilemap::load`.

## Format auto-detection

`Tilemap::load` inspects the first byte of the stream to choose between the two formats:

- If the first byte is an ASCII digit (`0`–`9`), the stream is parsed as **text format**.
- Otherwise, the stream is parsed as **binary format**.

---

## Text format

The text format is human-readable and suitable for hand-editing maps.

### Structure

```text
<width> <height>
<row 0>
<row 1>
...
<row height-1>
```

- The first line contains two decimal integers: `width` and `height`, separated by a space.
- Each subsequent line represents one row of the map, from top (y = 0) to bottom (y = height − 1).
- Each row must contain exactly `width` non-whitespace tile characters. Whitespace within a row is stripped before parsing, so spaces between characters are cosmetic (as produced by `saveAsText`) but not required.

### Tile characters

| Character | Meaning |
|---|---|
| `.` | Empty terrain |
| `~` | Water terrain |
| `#` | Obstacle terrain |
| `$` | Resource tile (empty terrain + `is_resource = true`) |
| `B` | Base tile (empty terrain + `is_base = true`) |

### Base side assignment

The text format does not encode which player owns a base. Side assignment is inferred automatically during loading:

- The **first** encountered `B`-region (scanned row-major, top-left to bottom-right) is assigned to **player 1**.
- The **second** encountered `B`-region is assigned to **player 2**.

Validation rules enforced at load time:

- Exactly two base regions must exist.
- Each base region must form a perfect square.
- Both bases must be the same size.

The inferred `base_size` is stored in the loaded `Tilemap` object.

### Example

A 6×4 map with two 2×2 bases in opposite corners, a 2×2 resource cluster, water, and an obstacle:

```text
6 4
BB.$$#
BB.$$.
....BB
....BB
```

Player 1's base occupies the top-left 2×2 area (columns 0–1, rows 0–1). Player 2's base occupies the bottom-right 2×2 area (columns 4–5, rows 2–3). The `$$` tiles at columns 3–4, rows 0–1 form a 2×2 resource cluster.

---

## Binary format

The binary format is compact and suitable for storage and network transfer.

### Byte order

All multi-byte integer fields are **little-endian**.

### Layout

```text
+-------------------+
| Magic (6 bytes)   |
+-------------------+
| Version (1 byte)  |
+-------------------+
| Header            |
+-------------------+
| Tile data         |
+-------------------+
```

### Magic and version

| Field   | Type  | Size | Value                       |
|---------|-------|-----:|-----------------------------|
| magic   | bytes |    6 | ASCII `CRTMAP` (0x43 0x52 0x54 0x4D 0x41 0x50) |
| version | u8    |    1 | Currently `2`               |

### Header

Immediately follows the version byte:

| Field      | Type | Size | Notes                        |
|------------|------|-----:|------------------------------|
| width      | u16  |    2 | Map width in tiles           |
| height     | u16  |    2 | Map height in tiles          |
| base_size  | u16  |    2 | Side length of each base square |
| reserved   | u16  |    2 | Reserved, must be `0`        |

Validation rules enforced at load time:

- `width` and `height` must each be between 1 and 512 (inclusive).

### Tile data

Immediately follows the header. Contains exactly `width × height` bytes, stored in row-major order (row y = 0 first, within each row x = 0 first).

Each byte encodes one tile as a packed bitfield:

| Bits   | Field        | Notes                                       |
|--------|--------------|---------------------------------------------|
| `[0:1]`| `terrain`    | `0` = Empty, `1` = Water, `3` = Obstacle    |
| `[2:3]`| `side`       | `0` = none, `1` = player 1, `2` = player 2 |
| `[4]`  | `is_resource`| `1` if the tile is a resource zone          |
| `[5]`  | `is_base`    | `1` if the tile belongs to a base           |
| `[6:7]`| reserved     | Must be `0`; ignored on read                |

Validation rules enforced at load time:

- `terrain` must be `0`, `1`, or `3`. Value `2` is invalid.
- `side` must be `0`, `1`, or `2`. Value `3` is invalid.
- Any tile with `side ≠ 0` must also have `is_base = 1`.
- Both player bases must be present, form perfect squares of size `base_size`, and must not exceed the map bounds.

### No trailing data

After the last tile byte, the stream must be at EOF. Any trailing bytes are treated as a format error.
