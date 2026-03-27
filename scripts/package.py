#!/usr/bin/env python3
"""Build selected starter components and bundle starter package for players.

Outputs:
  build/corertt-starter.zip

Contents:
  docs/rules.md
  docs/api-spec.md
	(optional) docs/corelib.md + corelib artifacts/sources
	(optional) rusty-corelib sources/config/docs

Usage:
  python scripts/package.py
	python scripts/package.py --without-corelib
	python scripts/package.py --without-rusty-corelib
  python scripts/package.py --output build/custom.zip
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import zipfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
BUILD_DIR = REPO_ROOT / "build"
CORELIB_DIR = REPO_ROOT / "corelib"
RUSTY_CORELIB_DIR = REPO_ROOT / "rusty-corelib"
DOCS_DIR = REPO_ROOT / "docs"

DEFAULT_ZIP_NAME = "corertt-starter.zip"

BASE_REQUIRED_DOCS = ["rules.md", "api-spec.md"]
CORELIB_DOC = "corelib.md"
REQUIRED_CORELIB_FILES = ["link.ld", "corelib.h", "corelib.c"]
CORELIB_ARTIFACT = "libcorelib.a"
TEST_DUMMY_SOURCE = CORELIB_DIR / "test" / "dummy.c"
RUSTY_CORELIB_ROOT_FILES = ["Cargo.toml", "Cargo.lock", "link.ld", "README.md"]
RUSTY_CORELIB_CARGO_CONFIG = RUSTY_CORELIB_DIR / ".cargo" / "config.toml"

MAKEFILE_FOR_PACKAGE = """# Starter Makefile for writing programs against corelib
# Supports both default GNU and optional Clang toolchain (set CLANG=1 to enable).

# ISA specification (can be overridden via command line)
MARCH = rv32g_zba_zbb_zbc_zbs

ifdef CLANG
CC = clang
AR = llvm-ar
CFLAGS += -target riscv32-none-elf
LDFLAGS += -fuse-ld=lld -Wl,--no-rosegment
else
CC = riscv64-elf-gcc
AR = riscv32-elf-ar
endif

# Compiler flags
CFLAGS += -O2 -ffreestanding -nostdlib -nostdinc \
		  -march=$(MARCH) -mabi=ilp32d \
		  -Wall -Icorelib

# Linker flags for executables
LDFLAGS += -static -Wl,--gc-sections -T corelib/link.ld
LDLIBS = -Lcorelib -lcorelib

SRC = src/base.c src/unit.c
OBJ = $(SRC:.c=.o)
ELF = $(SRC:.c=.elf)

all: $(ELF)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

%.elf: %.o corelib/libcorelib.a
	$(CC) $(CFLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -f src/*.o src/*.elf src/*.d
"""

def build_corelib():
	print("[package] Building corelib...")
	if not CORELIB_DIR.exists() or not CORELIB_DIR.is_dir():
		raise FileNotFoundError(f"corelib directory not found: {CORELIB_DIR}")
	subprocess.run(["make", "-C", str(CORELIB_DIR)], check=True)


def collect_rusty_corelib_files() -> list[tuple[Path, Path]]:
	if not RUSTY_CORELIB_DIR.exists() or not RUSTY_CORELIB_DIR.is_dir():
		raise FileNotFoundError(f"rusty-corelib directory not found: {RUSTY_CORELIB_DIR}")

	files: list[tuple[Path, Path]] = []
	for file_name in RUSTY_CORELIB_ROOT_FILES:
		src = RUSTY_CORELIB_DIR / file_name
		if not src.exists():
			raise FileNotFoundError(f"Missing rusty-corelib file: {src}")
		files.append((src, Path("rusty-corelib") / file_name))

	api_doc = DOCS_DIR / "rusty-corelib.md"
	if not api_doc.exists():
		raise FileNotFoundError(f"Missing rusty-corelib API documentation file: {api_doc}")

	files.append((api_doc, Path("docs") / "rusty-corelib.md"))

	if not RUSTY_CORELIB_CARGO_CONFIG.exists():
		raise FileNotFoundError(f"Missing rusty-corelib config file: {RUSTY_CORELIB_CARGO_CONFIG}")
	files.append((RUSTY_CORELIB_CARGO_CONFIG, Path("rusty-corelib") / ".cargo" / "config.toml"))

	src_dir = RUSTY_CORELIB_DIR / "src"
	if not src_dir.exists() or not src_dir.is_dir():
		raise FileNotFoundError(f"Missing rusty-corelib src directory: {src_dir}")

	for src in sorted(src_dir.rglob("*")):
		if not src.is_file():
			continue
		rel = src.relative_to(RUSTY_CORELIB_DIR)
		files.append((src, Path("rusty-corelib") / rel))

	hello_world = RUSTY_CORELIB_DIR / "examples" / "hello_world.rs"
	if hello_world.exists():
		files.append((hello_world, Path("rusty-corelib") / "examples" / "hello_world.rs"))

	return files


def collect_files(output_zip: Path, include_corelib: bool, include_rusty_corelib: bool):
	if not BUILD_DIR.exists():
		BUILD_DIR.mkdir(parents=True, exist_ok=True)

	if not include_corelib and not include_rusty_corelib:
		raise ValueError("At least one component must be included: corelib or rusty-corelib")

	src_targets = []
	for doc in BASE_REQUIRED_DOCS:
		p = DOCS_DIR / doc
		if not p.exists():
			raise FileNotFoundError(f"Missing documentation file: {p}")
		src_targets.append((p, Path("docs") / doc))

	if include_corelib:
		corelib_doc = DOCS_DIR / CORELIB_DOC
		if not corelib_doc.exists():
			raise FileNotFoundError(f"Missing documentation file: {corelib_doc}")
		src_targets.append((corelib_doc, Path("docs") / CORELIB_DOC))

		for f in REQUIRED_CORELIB_FILES:
			p = CORELIB_DIR / f
			if not p.exists():
				raise FileNotFoundError(f"Missing corelib source file: {p}")
			src_targets.append((p, Path("corelib") / f))

		artifact = CORELIB_DIR / CORELIB_ARTIFACT
		if not artifact.exists():
			raise FileNotFoundError(f"Missing corelib artifact file: {artifact}. Did you run build?")
		src_targets.append((artifact, Path("corelib") / CORELIB_ARTIFACT))

	if include_rusty_corelib:
		src_targets.extend(collect_rusty_corelib_files())

	dummy_bytes = b""
	if include_corelib:
		if not TEST_DUMMY_SOURCE.exists():
			raise FileNotFoundError(f"Missing test dummy source file: {TEST_DUMMY_SOURCE}")

		with open(TEST_DUMMY_SOURCE, "rb") as f:
			dummy_bytes = f.read()

	included_components = [
		"corelib (C)" if include_corelib else None,
		"rusty-corelib (Rust)" if include_rusty_corelib else None,
	]
	included_components_text = "\n".join(f"- {name}" for name in included_components if name is not None)

	readme_lines = [
		"# CoreRTT Starter Package",
		"",
		"This package contains selected starter components for CoreRTT players.",
		"",
		"Included component groups:",
		included_components_text,
		"",
		"Included docs:",
		"- docs/rules.md",
		"- docs/api-spec.md",
	]

	if include_corelib:
		readme_lines.extend([
			"- docs/corelib.md",
			"",
			"Included C starter files:",
			"- corelib/libcorelib.a",
			"- corelib/link.ld",
			"- corelib/corelib.h",
			"- corelib/corelib.c",
			"- src/base.c (copy of test/dummy.c)",
			"- src/unit.c (copy of test/dummy.c)",
			"- Makefile (starter build for src/base.c and src/unit.c)",
		])

	if include_rusty_corelib:
		readme_lines.extend([
			"",
			"Included Rust starter files:",
			"- docs/rusty-corelib.md (API documentation for rusty-corelib)",
			"- rusty-corelib/Cargo.toml",
			"- rusty-corelib/Cargo.lock",
			"- rusty-corelib/link.ld",
			"- rusty-corelib/README.md",
			"- rusty-corelib/.cargo/config.toml",
			"- rusty-corelib/src/*",
			"- rusty-corelib/examples/hello_world.rs (if present)",
		])

	readme_lines.extend([
		"",
		"Usage:",
		"1. Extract archive",
	])

	if include_corelib:
		readme_lines.extend([
			"2. Run `make` to produce `src/base.elf` and `src/unit.elf`",
			"3. Run `make clean` to remove outputs",
			"",
			"Optional:",
			"- set `CLANG=1` for Clang/llvm-ar toolchain in the Makefile",
		])

	if include_rusty_corelib:
		readme_lines.extend([
			"",
			"Rust workflow example:",
			"- cd rusty-corelib",
			"- cargo build --release --examples",
		])

	readme_text = "\n".join(readme_lines).strip()

	with zipfile.ZipFile(output_zip, "w", compression=zipfile.ZIP_DEFLATED) as zf:
		for src, rel in src_targets:
			print(f"[package] Adding {src} as {rel}")
			zf.write(src, arcname=str(rel))

		if include_corelib:
			print(f"[package] Adding generated src/base.c and src/unit.c")
			zf.writestr("src/base.c", dummy_bytes)
			zf.writestr("src/unit.c", dummy_bytes)

		if include_corelib:
			print(f"[package] Adding generated Makefile")
			zf.writestr("Makefile", MAKEFILE_FOR_PACKAGE)

		readme_name = "README.md"
		zf.writestr(readme_name, readme_text)
		print(f"[package] Wrote helpful README file: {readme_name}")

	print(f"[package] Package created: {output_zip} ({output_zip.stat().st_size} bytes)")


def main() -> int:
	parser = argparse.ArgumentParser(description="Build selected components and create a starter zip package")
	parser.add_argument("--output", "-o", default=str(BUILD_DIR / DEFAULT_ZIP_NAME),
						help="Location of output zip file")
	parser.add_argument("--no-build", action="store_true", help="Skip corelib build step")
	parser.add_argument("--with-corelib", dest="with_corelib", action="store_true",
						help="Include C corelib files (default)")
	parser.add_argument("--without-corelib", dest="with_corelib", action="store_false",
						help="Exclude C corelib files")
	parser.set_defaults(with_corelib=True)
	parser.add_argument("--with-rusty-corelib", dest="with_rusty_corelib", action="store_true",
						help="Include rusty-corelib files (default)")
	parser.add_argument("--without-rusty-corelib", dest="with_rusty_corelib", action="store_false",
						help="Exclude rusty-corelib files")
	parser.set_defaults(with_rusty_corelib=True)
	args = parser.parse_args()

	output_zip = Path(args.output).resolve()
	if not output_zip.parent.exists():
		output_zip.parent.mkdir(parents=True, exist_ok=True)

	if args.with_corelib and not args.no_build:
		build_corelib()

	collect_files(output_zip, include_corelib=args.with_corelib,
				  include_rusty_corelib=args.with_rusty_corelib)
	return 0


if __name__ == "__main__":
	try:
		raise SystemExit(main())
	except subprocess.CalledProcessError as exc:
		print(f"[error] Command failed: {exc}", file=sys.stderr)
		sys.exit(exc.returncode)
	except Exception as exc:
		print(f"[error] {exc}", file=sys.stderr)
		sys.exit(1)
