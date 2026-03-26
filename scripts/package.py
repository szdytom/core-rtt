#!/usr/bin/env python3
"""Build corelib and bundle starter package for players.

Outputs:
  build/corertt-starter.zip

Contents:
  docs/rules.md
  docs/api-spec.md
  docs/corelib.md
  corelib/libcorelib.a
  corelib/link.ld
  corelib/corelib.h
  corelib/corelib.c

Usage:
  python scripts/package.py
  python scripts/package.py --output build/custom.zip
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
BUILD_DIR = REPO_ROOT / "build"
CORELIB_DIR = REPO_ROOT / "corelib"
DOCS_DIR = REPO_ROOT / "docs"

DEFAULT_ZIP_NAME = "corertt-starter.zip"

REQUIRED_DOCS = ["rules.md", "api-spec.md", "corelib.md"]
REQUIRED_CORELIB_FILES = ["link.ld", "corelib.h", "corelib.c"]
CORELIB_ARTIFACT = "libcorelib.a"
TEST_DUMMY_SOURCE = CORELIB_DIR / "test" / "dummy.c"

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

def collect_files(output_zip: Path):
    if not BUILD_DIR.exists():
        BUILD_DIR.mkdir(parents=True, exist_ok=True)

    src_targets = []
    for doc in REQUIRED_DOCS:
        p = DOCS_DIR / doc
        if not p.exists():
            raise FileNotFoundError(f"Missing documentation file: {p}")
        src_targets.append((p, Path("docs") / doc))

    for f in REQUIRED_CORELIB_FILES:
        p = CORELIB_DIR / f
        if not p.exists():
            raise FileNotFoundError(f"Missing corelib source file: {p}")
        src_targets.append((p, Path("corelib") / f))

    artifact = CORELIB_DIR / CORELIB_ARTIFACT
    if not artifact.exists():
        raise FileNotFoundError(f"Missing corelib artifact file: {artifact}. Did you run build?")
    src_targets.append((artifact, Path("corelib") / CORELIB_ARTIFACT))

    if not TEST_DUMMY_SOURCE.exists():
        raise FileNotFoundError(f"Missing test dummy source file: {TEST_DUMMY_SOURCE}")

    with open(TEST_DUMMY_SOURCE, "rb") as f:
        dummy_bytes = f.read()

    readme_text = """# CoreRTT Starter Package

This is a minimal starter package for CoreRTT programming players.
It includes docs, corelib artifacts and two sample source programs.

Included files:
- docs/rules.md
- docs/api-spec.md
- docs/corelib.md
- corelib/libcorelib.a
- corelib/link.ld
- corelib/corelib.h
- corelib/corelib.c
- src/base.c (copy of test/dummy.c)
- src/unit.c (copy of test/dummy.c)
- Makefile (starter build for the two src files)

Usage:
1. Extract archive
2. Run `make` to produce `src/base.elf` and `src/unit.elf`
3. Run `make clean` to remove outputs

Optional:
- set `CLANG=1` for Clang/llvm-ar toolchain in the Makefile
""".strip()

    with zipfile.ZipFile(output_zip, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for src, rel in src_targets:
            print(f"[package] Adding {src} as {rel}")
            zf.write(src, arcname=str(rel))

        print(f"[package] Adding generated src/base.c and src/unit.c")
        zf.writestr("src/base.c", dummy_bytes)
        zf.writestr("src/unit.c", dummy_bytes)

        print(f"[package] Adding generated Makefile")
        zf.writestr("Makefile", MAKEFILE_FOR_PACKAGE)

        readme_name = "README.md"
        zf.writestr(readme_name, readme_text)
        print(f"[package] Wrote helpful README file: {readme_name}")

    print(f"[package] Package created: {output_zip} ({output_zip.stat().st_size} bytes)")


def main() -> int:
    parser = argparse.ArgumentParser(description="Build corelib and create a starter zip package")
    parser.add_argument("--output", "-o", default=str(BUILD_DIR / DEFAULT_ZIP_NAME),
                        help="Location of output zip file")
    parser.add_argument("--no-build", action="store_true", help="Skip corelib build step")
    args = parser.parse_args()

    output_zip = Path(args.output).resolve()
    if not output_zip.parent.exists():
        output_zip.parent.mkdir(parents=True, exist_ok=True)

    if not args.no_build:
        build_corelib()

    collect_files(output_zip)
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
