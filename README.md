# py-tuntap-abi3

Native CPython `abi3` TUN/TAP extension for Linux, macOS, and Windows.

## Packaging model

- Built as platform-specific wheels with CPython limited ABI (`cp38-abi3`).
- Linux/macOS wheels contain only native extension artifacts for those platforms.
- Windows wheels contain the native extension plus one matching-arch `wintun.dll`.
- `wintun.dll` payloads are not committed to this repository.

## Wintun release sourcing

- Wintun binaries are pulled from `https://github.com/dryark/wintun` releases.
- Release tag and SHA256 values are pinned in `packaging/wintun-release.json`.
- `scripts/fetch_wintun_release.py` verifies hashes before staging DLLs.
- `scripts/check_no_tracked_wintun_dlls.py` ensures DLL payloads are not committed.

## License

- Project license file: `FC_LICENSE`
- Dry Ark LLC authored files include per-file Fair Coding License 1.0+ declarations.
