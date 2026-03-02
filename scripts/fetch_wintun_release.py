#!/usr/bin/env python3
# Copyright (C) 2026 Dry Ark LLC
# License: Fair Coding License 1.0+
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

HEX64_RE = re.compile(r"^[0-9a-fA-F]{64}$")


def fail(msg: str) -> None:
    raise SystemExit(msg)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Fetch pinned Wintun release assets, verify SHA256, and stage into package paths."
    )
    parser.add_argument(
        "--manifest",
        default="packaging/wintun-release.json",
        help="Path to wintun release pin manifest.",
    )
    parser.add_argument(
        "--arches",
        default="amd64,arm64,x86",
        help="Comma-separated architectures to fetch.",
    )
    parser.add_argument(
        "--output-root",
        default="py_tuntap_abi3/wintun/bin",
        help="Destination root directory for staged DLL and sidecar driver files.",
    )
    return parser.parse_args()


def load_manifest(path: Path) -> dict:
    if not path.exists():
        fail(f"manifest not found: {path}")
    with path.open("r", encoding="utf-8") as f:
        data = json.load(f)
    for key in ("repo", "tag", "assets"):
        if key not in data:
            fail(f"manifest missing key: {key}")
    return data


def expected_hash(sha256: str, arch: str) -> str:
    if sha256.startswith("REPLACE_WITH_SHA256_"):
        fail(
            f"manifest has placeholder SHA256 for {arch}; "
            "publish dryark/wintun release and replace hash values first"
        )
    if not HEX64_RE.match(sha256):
        fail(f"invalid SHA256 for {arch}: {sha256}")
    return sha256.lower()


def download_file(url: str, destination: Path) -> None:
    headers = {
        "User-Agent": "py-tuntap-abi3-fetch-wintun",
        "Accept": "application/octet-stream",
    }
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req) as response, destination.open("wb") as f:
        shutil.copyfileobj(response, f)


def file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def extract_bundle(downloaded_zip: Path, output_root: Path, arch: str) -> Path:
    bundle_members = {
        "amd64": {
            "dll": "drywintun-amd64.dll",
            "sys": "DRYWINTUN-AMD64.sys",
            "inf": "DRYWINTUN-AMD64.inf",
            "cat": "DRYWINTUN-AMD64.cat",
        },
        "arm64": {
            "dll": "drywintun-arm64.dll",
            "sys": "DRYWINTUN-ARM64.sys",
            "inf": "DRYWINTUN-ARM64.inf",
            "cat": "DRYWINTUN-ARM64.cat",
        },
        "x86": {
            "dll": "drywintun-x86.dll",
            "sys": "DRYWINTUN-X86.sys",
            "inf": "DRYWINTUN-X86.inf",
            "cat": "DRYWINTUN-X86.cat",
        },
    }
    member_map = bundle_members.get(arch)
    if member_map is None:
        fail(f"unsupported arch for bundle extraction: {arch}")

    dest_dir = output_root / arch
    dest_dir.mkdir(parents=True, exist_ok=True)

    dest_paths = {
        "dll": dest_dir / "drywintun.dll",
        "sys": dest_dir / "drywintun.sys",
        "inf": dest_dir / "drywintun.inf",
        "cat": dest_dir / "drywintun.cat",
    }

    with zipfile.ZipFile(downloaded_zip, "r") as zf:
        names = set(zf.namelist())
        for kind, src_name in member_map.items():
            if src_name not in names:
                fail(
                    f"bundle {downloaded_zip.name} missing expected member for {arch}: "
                    f"{src_name}"
                )
            with zf.open(src_name, "r") as src, dest_paths[kind].open("wb") as dst:
                shutil.copyfileobj(src, dst)
    return dest_paths["dll"]


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parents[1]
    manifest_path = (repo_root / args.manifest).resolve()
    output_root = (repo_root / args.output_root).resolve()

    manifest = load_manifest(manifest_path)
    repo = manifest["repo"]
    tag = manifest["tag"]
    assets = manifest["assets"]

    arches = [a.strip() for a in args.arches.split(",") if a.strip()]
    if not arches:
        fail("no architectures requested")

    with tempfile.TemporaryDirectory(prefix="wintun_fetch_") as td:
        temp_dir = Path(td)
        for arch in arches:
            if arch not in assets:
                fail(f"manifest missing asset mapping for arch: {arch}")

            entry = assets[arch]
            filename = entry.get("filename")
            sha256 = entry.get("sha256")
            if not filename or not sha256:
                fail(f"manifest asset entry incomplete for arch: {arch}")

            expected = expected_hash(sha256, arch)
            url = f"https://github.com/{repo}/releases/download/{tag}/{filename}"
            downloaded = temp_dir / filename
            print(f"fetching {arch}: {url}")
            download_file(url, downloaded)
            actual = file_sha256(downloaded)
            if actual != expected:
                fail(
                    f"SHA256 mismatch for {filename}: expected {expected}, got {actual}"
                )
            staged = extract_bundle(downloaded, output_root, arch)
            print(
                f"staged {arch} -> {staged.parent}"
                " (drywintun.dll + drywintun.{sys,inf,cat})"
            )

    return 0


if __name__ == "__main__":
    sys.exit(main())
