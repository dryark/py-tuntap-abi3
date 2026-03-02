# Copyright (C) 2026 Dry Ark LLC
# License: Fair Coding License 1.0+
from __future__ import annotations

import os

import pytest


def _is_root() -> bool:
    geteuid = getattr(os, "geteuid", None)
    if geteuid is None:
        return False
    return geteuid() == 0


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line(
        "markers",
        "tunnel_macos: privileged macOS integration test that exercises real tunnel traffic",
    )
    config.addinivalue_line(
        "markers",
        "tunnel_linux: privileged Linux integration test that exercises real tunnel traffic",
    )


def pytest_collection_modifyitems(config: pytest.Config, items: list[pytest.Item]) -> None:
    enabled = os.environ.get("PYTUN_RUN_TUNNEL_TESTS") == "1"
    root = _is_root()

    for item in items:
        is_tunnel_test = "tunnel_macos" in item.keywords or "tunnel_linux" in item.keywords
        if not is_tunnel_test:
            continue

        if not enabled:
            item.add_marker(
                pytest.mark.skip(
                    reason="set PYTUN_RUN_TUNNEL_TESTS=1 to run privileged tunnel integration tests"
                )
            )
            continue

        if not root:
            item.add_marker(
                pytest.mark.skip(reason="tunnel integration tests require root privileges")
            )
