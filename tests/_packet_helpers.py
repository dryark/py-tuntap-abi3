# Copyright (C) 2026 Dry Ark LLC
# License: Fair Coding License 1.0+
from __future__ import annotations

import ipaddress
import struct

AF_INET6 = 30
ETH_P_IPV6 = 0x86DD
IPPROTO_ICMPV6 = 58
ICMPV6_ECHO_REQUEST = 128
ICMPV6_ECHO_REPLY = 129


def _inet6(addr: str) -> bytes:
    return ipaddress.IPv6Address(addr).packed


def strip_tun_prefix(frame: bytes, platform_name: str) -> bytes:
    if platform_name == "linux":
        if len(frame) < 4:
            raise ValueError("linux tun frame too short")
        # Linux TUN (without IFF_NO_PI) prepends 4 bytes: flags(2), proto(2).
        proto = struct.unpack("!H", frame[2:4])[0]
        if proto != ETH_P_IPV6:
            raise ValueError(f"linux tun frame is not IPv6 (proto=0x{proto:04x})")
        return frame[4:]

    if platform_name == "darwin":
        if len(frame) < 4:
            raise ValueError("darwin utun frame too short")
        # UTUN prepends 4 bytes with address family (endianness varies by path/tooling).
        family_be = struct.unpack("!I", frame[:4])[0]
        family_le = struct.unpack("<I", frame[:4])[0]
        if family_be == AF_INET6 or family_le == AF_INET6:
            return frame[4:]
        raise ValueError(f"darwin utun frame is not AF_INET6 (be={family_be}, le={family_le})")

    return frame


def add_tun_prefix(ipv6_packet: bytes, platform_name: str) -> bytes:
    if platform_name == "linux":
        return struct.pack("!HH", 0, ETH_P_IPV6) + ipv6_packet
    if platform_name == "darwin":
        return struct.pack("!I", AF_INET6) + ipv6_packet
    return ipv6_packet


def parse_ipv6_icmpv6_echo(frame: bytes, platform_name: str) -> dict[str, int | bytes]:
    pkt = strip_tun_prefix(frame, platform_name)
    if len(pkt) < 48:
        raise ValueError("IPv6 packet too short for ICMPv6 echo")

    version = pkt[0] >> 4
    if version != 6:
        raise ValueError(f"not IPv6 packet version={version}")

    payload_len = struct.unpack("!H", pkt[4:6])[0]
    next_header = pkt[6]
    if next_header != IPPROTO_ICMPV6:
        raise ValueError(f"not ICMPv6 next_header={next_header}")

    if len(pkt) < 40 + payload_len:
        raise ValueError("truncated IPv6 payload")

    src = pkt[8:24]
    dst = pkt[24:40]
    icmp = pkt[40 : 40 + payload_len]
    if len(icmp) < 8:
        raise ValueError("ICMPv6 echo payload too short")

    icmp_type = icmp[0]
    ident, seq = struct.unpack("!HH", icmp[4:8])
    data = icmp[8:]
    return {
        "src": src,
        "dst": dst,
        "icmp_type": icmp_type,
        "ident": ident,
        "seq": seq,
        "data": data,
    }


def _checksum(data: bytes) -> int:
    if len(data) % 2 == 1:
        data += b"\x00"
    total = 0
    for i in range(0, len(data), 2):
        total += (data[i] << 8) + data[i + 1]
        total = (total & 0xFFFF) + (total >> 16)
    return (~total) & 0xFFFF


def build_icmpv6_echo_reply_frame(
    request_frame: bytes, platform_name: str, expected_local_addr: str, expected_peer_addr: str
) -> bytes:
    parsed = parse_ipv6_icmpv6_echo(request_frame, platform_name)
    if parsed["icmp_type"] != ICMPV6_ECHO_REQUEST:
        raise ValueError(f"not ICMPv6 echo request type={parsed['icmp_type']}")

    src = parsed["src"]
    dst = parsed["dst"]
    if src != _inet6(expected_local_addr) and src != _inet6(expected_peer_addr):
        raise ValueError("request src address unexpected")
    if dst != _inet6(expected_local_addr) and dst != _inet6(expected_peer_addr):
        raise ValueError("request dst address unexpected")

    payload = struct.pack(
        "!BBHHH",
        ICMPV6_ECHO_REPLY,
        0,
        0,
        int(parsed["ident"]),
        int(parsed["seq"]),
    ) + bytes(parsed["data"])
    payload_len = len(payload)
    src_reply = dst
    dst_reply = src

    ipv6_header = struct.pack(
        "!IHBB16s16s",
        (6 << 28),
        payload_len,
        IPPROTO_ICMPV6,
        64,
        src_reply,
        dst_reply,
    )

    pseudo = src_reply + dst_reply + struct.pack("!I3xB", payload_len, IPPROTO_ICMPV6)
    checksum = _checksum(pseudo + payload)
    payload = struct.pack(
        "!BBHHH",
        ICMPV6_ECHO_REPLY,
        0,
        checksum,
        int(parsed["ident"]),
        int(parsed["seq"]),
    ) + bytes(parsed["data"])
    return add_tun_prefix(ipv6_header + payload, platform_name)
