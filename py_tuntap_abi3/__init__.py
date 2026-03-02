# Copyright (C) 2026 Dry Ark LLC
# License: Fair Coding License 1.0+
"""Public package API for py_tuntap_abi3."""

from __future__ import annotations

import os
import platform
import sys
from pathlib import Path


def _windows_arch() -> str:
    value = os.environ.get("PYTUN_WINTUN_ARCH", platform.machine())
    normalized = value.lower().replace("-", "").replace("_", "")
    mapping = {
        "amd64": "amd64",
        "x8664": "amd64",
        "arm64": "arm64",
        "aarch64": "arm64",
        "x86": "x86",
        "i386": "x86",
        "i686": "x86",
        "win32": "x86",
        "arm": "arm",
        "armv7": "arm",
        "armv7l": "arm",
    }
    return mapping.get(normalized, normalized)


def _configure_windows_dll_path() -> None:
    if sys.platform != "win32":
        return
    dll_dir = Path(__file__).resolve().parent / "wintun" / "bin" / _windows_arch()
    if dll_dir.is_dir():
        os.add_dll_directory(str(dll_dir))


_configure_windows_dll_path()

from ._pytun import IFF_TAP, IFF_TUN, TunTapDevice

__all__ = ["IFF_TAP", "IFF_TUN", "TunTapDevice"]
