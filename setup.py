# Copyright (C) 2026 Dry Ark LLC
# License: Fair Coding License 1.0+

import os
import platform
import sys
import sysconfig

from setuptools import Extension, setup


def _backend_source() -> str:
    if sys.platform == "linux":
        return "src/native/linux_backend.c"
    if sys.platform == "darwin":
        return "src/native/darwin_backend.c"
    if sys.platform == "win32":
        return "src/native/windows_backend.c"
    raise RuntimeError(f"Unsupported platform: {sys.platform}")


def _normalize_windows_arch(name: str) -> str:
    n = name.lower().replace("-", "").replace("_", "")
    if n in {"amd64", "x8664"}:
        return "amd64"
    if n in {"arm64", "aarch64"}:
        return "arm64"
    if n in {"x86", "i386", "i686", "win32"}:
        return "x86"
    if n in {"arm", "armv7", "armv7l"}:
        return "arm"
    raise RuntimeError(f"Unknown windows arch value: {name}")


def _windows_package_data() -> list[str]:
    # Prefer the build target platform tag (e.g. win-amd64/win32/win-arm64).
    plat = sysconfig.get_platform().lower()
    if "win-amd64" in plat:
        arch = "amd64"
    elif "win-arm64" in plat:
        arch = "arm64"
    elif "win32" in plat:
        arch = "x86"
    else:
        # Optional override/fallback for non-standard build environments.
        arch = os.environ.get("PYTUN_WINTUN_ARCH") or platform.machine()
    normalized = _normalize_windows_arch(arch)
    rel_path = f"wintun/bin/{normalized}/wintun.dll"
    absolute = os.path.join(os.path.dirname(__file__), "py_tuntap_abi3", rel_path)
    if not os.path.exists(absolute):
        raise RuntimeError(
            f"missing required Wintun asset: {rel_path}. "
            "Run scripts/fetch_wintun_release.py before building Windows wheels."
        )
    return [rel_path]


ext = Extension(
    "py_tuntap_abi3._pytun",
    sources=["src/native/module.c", _backend_source()],
    define_macros=[("Py_LIMITED_API", "0x03080000")],
    py_limited_api=True,
)

kwargs = {
    "ext_modules": [ext],
    "zip_safe": False,
    "options": {"bdist_wheel": {"py_limited_api": "cp38"}},
}

if sys.platform == "win32":
    kwargs["package_data"] = {"py_tuntap_abi3": _windows_package_data()}
else:
    kwargs["package_data"] = {"py_tuntap_abi3": []}

setup(**kwargs)
