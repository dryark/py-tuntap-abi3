# Copyright (C) 2026 Dry Ark LLC
# License: Fair Coding License 1.0+

import subprocess
import sys
import tempfile


def test_native_symbols_are_available():
    with tempfile.TemporaryDirectory() as td:
        proc = subprocess.run(
            [
                sys.executable,
                "-c",
                (
                    "import py_tuntap_abi3;"
                    "assert hasattr(py_tuntap_abi3, 'TunTapDevice');"
                    "assert hasattr(py_tuntap_abi3, 'IFF_TUN');"
                    "assert hasattr(py_tuntap_abi3, 'IFF_TAP')"
                ),
            ],
            cwd=td,
            check=False,
            capture_output=True,
            text=True,
        )
    assert proc.returncode == 0, proc.stderr
