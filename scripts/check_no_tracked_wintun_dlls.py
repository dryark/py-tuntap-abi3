#!/usr/bin/env python3
# Copyright (C) 2026 Dry Ark LLC
# License: Fair Coding License 1.0+
from __future__ import annotations

import sys
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    matches = sorted(root.glob("py_tuntap_abi3/wintun/bin/**/*.dll"))
    if matches:
        print("Wintun DLL payloads must not exist in source checkout:")
        for path in matches:
            rel = path.relative_to(root)
            print(f" - {rel}")
        print("CI should hydrate these during Windows wheel builds.")
        return 1
    print("ok: no Wintun DLL payloads present in source checkout")
    return 0


if __name__ == "__main__":
    sys.exit(main())
