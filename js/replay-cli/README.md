# replay-cli

CLI utility for extracting replay logs from core-rtt replay binaries using `@corertt/corereplay`.

## Build

```sh
npm install
npm run build
```

## Usage

```sh
corertt-replay-log --replay-file ./replay.dat --format jsonl
```

Options:

- `--replay-file <path>`: Input replay file path (required).
- `--output <path>`: Output file path. Writes to stdout when omitted.
- `--format <jsonl|text>`: Output format, default is `jsonl`.

## Output Modes

- `jsonl`: one JSON object per line.
- `text`: human-readable multiline log format aligned with the C++ extractor semantics.

JSONL fields:

- `tick`
- `player_id`
- `unit_id`
- `source` (`SYS` or `USR`)
- `type` (`custom`, `unit_creation`, `unit_destruction`, `execution_exception`, `base_captured`)
- `payload_text`
- `payload_hex`
