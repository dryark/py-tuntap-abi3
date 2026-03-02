# Copyright (C) 2026 Dry Ark LLC
# License: Fair Coding License 1.0+
from __future__ import annotations

import select
import shutil
import subprocess
import sys
import time
import uuid

import pytest

from py_tuntap_abi3 import TunTapDevice

from ._packet_helpers import (
    ICMPV6_ECHO_REQUEST,
    build_icmpv6_echo_reply_frame,
    parse_ipv6_icmpv6_echo,
)

pytestmark = [
    pytest.mark.tunnel_linux,
    pytest.mark.skipif(sys.platform != "linux", reason="Linux-only integration test"),
]


def _require_commands() -> None:
    for cmd in ("ip", "ping"):
        if shutil.which(cmd) is None:
            pytest.skip(f"required command not found: {cmd}")


def _random_ipv6_pair() -> tuple[str, str]:
    subnet = int(uuid.uuid4().hex[:4], 16)
    return f"fd00:{subnet:04x}::1", f"fd00:{subnet:04x}::2"


def _wait_for_echo_request(dev: TunTapDevice, timeout_s: float) -> bytes:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        ready, _, _ = select.select([dev.fileno()], [], [], 0.25)
        if not ready:
            continue
        frame = dev.read(4096)
        try:
            parsed = parse_ipv6_icmpv6_echo(frame, "linux")
        except ValueError:
            continue
        if parsed["icmp_type"] == ICMPV6_ECHO_REQUEST:
            return frame
    raise TimeoutError("timed out waiting for Linux ICMPv6 echo request on tun fd")


def test_linux_tun_lifecycle_and_config():
    _require_commands()
    if shutil.which("ip") is None:
        pytest.skip("ip command is required for Linux backend address setup")

    tun_name = f"pytun{uuid.uuid4().hex[:8]}"
    dev = TunTapDevice(name=tun_name)
    try:
        assert dev.name.startswith("pytun")
        dev.up()

        local_addr, _ = _random_ipv6_pair()
        dev.addr = local_addr
        dev.mtu = 1400

        assert dev.name.startswith("pytun")
        assert isinstance(dev.fileno(), int)
        assert dev.fileno() > 0
        assert str(dev.addr).lower() == local_addr.lower()
        assert int(dev.mtu) == 1400

        dev.down()
        dev.close()
        with pytest.raises(OSError):
            dev.fileno()
    finally:
        try:
            dev.close()
        except OSError:
            pass


def test_linux_tun_icmpv6_echo_data_path():
    _require_commands()
    local_addr, peer_addr = _random_ipv6_pair()
    tun_name = f"pytun{uuid.uuid4().hex[:8]}"
    dev = TunTapDevice(name=tun_name)
    ping_proc: subprocess.Popen[str] | None = None
    try:
        dev.up()
        dev.addr = local_addr
        dev.mtu = 1400

        ping_cmd = ["ping", "-6", "-n", "-c", "1", "-W", "3", "-I", dev.name, peer_addr]
        ping_proc = subprocess.Popen(
            ping_cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        request_frame = _wait_for_echo_request(dev, timeout_s=6.0)
        reply_frame = build_icmpv6_echo_reply_frame(
            request_frame,
            platform_name="linux",
            expected_local_addr=local_addr,
            expected_peer_addr=peer_addr,
        )
        wrote = dev.write(reply_frame)
        assert wrote == len(reply_frame)

        stdout, stderr = ping_proc.communicate(timeout=8)
        assert ping_proc.returncode == 0, f"ping failed\nstdout:\n{stdout}\nstderr:\n{stderr}"
    finally:
        if ping_proc is not None and ping_proc.poll() is None:
            ping_proc.kill()
            ping_proc.wait(timeout=3)
        try:
            dev.close()
        except OSError:
            pass
