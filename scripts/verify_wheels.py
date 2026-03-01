#!/usr/bin/env python3
# Copyright (C) 2026 Dry Ark LLC
# License: Fair Coding License 1.0+
from __future__ import annotations

import argparse
import sys
import zipfile
from pathlib import Path


def fail(msg: str) -> None:
    raise SystemExit(msg)


def has_abi3_tag(filename: str) -> bool:
    return "-abi3-" in filename


def is_pure_any(filename: str) -> bool:
    return filename.endswith("-py3-none-any.whl")


def check_wheel(path: Path) -> None:
    name = path.name
    if is_pure_any(name):
        fail(f"{name}: wheel tag must not be py3-none-any")
    if not has_abi3_tag(name):
        fail(f"{name}: expected abi3 tag")

    with zipfile.ZipFile(path) as zf:
        files = zf.namelist()
        dlls = [f for f in files if f.lower().endswith(".dll")]
        if "win" not in name:
            if dlls:
                fail(f"{name}: non-windows wheel must not contain DLLs ({dlls})")
            return

        arch_dirs = set()
        for dll in dlls:
            if not dll.startswith("py_tuntap_abi3/wintun/bin/"):
                continue
            parts = dll.split("/")
            if len(parts) >= 5:
                arch_dirs.add(parts[3])

        if len(arch_dirs) != 1:
            fail(f"{name}: expected exactly one wintun arch directory, got {sorted(arch_dirs)}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("wheel_dir", type=Path)
    args = parser.parse_args()

    wheel_dir = args.wheel_dir
    wheels = sorted(wheel_dir.glob("*.whl"))
    if not wheels:
        fail(f"no wheels found in {wheel_dir}")
    for wheel in wheels:
        check_wheel(wheel)
    print(f"verified {len(wheels)} wheel(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
