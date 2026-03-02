#!/usr/bin/env python3
# Copyright (C) 2026 Dry Ark LLC
# License: Fair Coding License 1.0+
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    patterns = (
        "py_tuntap_abi3/wintun/bin/**/*.dll",
        "py_tuntap_abi3/wintun/bin/**/*.sys",
        "py_tuntap_abi3/wintun/bin/**/*.inf",
        "py_tuntap_abi3/wintun/bin/**/*.cat",
    )
    cmd = ["git", "-C", str(root), "ls-files", "--", *patterns]
    result = subprocess.run(cmd, check=True, capture_output=True, text=True)
    tracked = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    if tracked:
        print("Wintun payload files must not be tracked in git:")
        for path in tracked:
            print(f" - {path}")
        print("CI should hydrate these during Windows wheel builds, but keep them untracked.")
        return 1
    print("ok: no tracked Wintun payload files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
