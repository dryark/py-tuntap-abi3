# py-tuntap-abi3

Native CPython `abi3` TUN/TAP extension for Linux, macOS, and Windows.

## Packaging model

- Built as platform-specific wheels with CPython limited ABI (`cp38-abi3`).
- Linux/macOS wheels contain only native extension artifacts for those platforms.
- Windows wheels contain the native extension plus matching-arch `drywintun` payloads.
- `drywintun.dll` payloads are not committed to this repository.

## Wintun release sourcing

- Wintun binaries are pulled from `https://github.com/dryark/wintun` releases.
- Release tag and SHA256 values are pinned in `packaging/wintun-release.json`.
- `scripts/fetch_wintun_release.py` verifies hashes before staging payloads.
- `scripts/check_no_tracked_wintun_dlls.py` ensures payload files are not committed.

## Windows payload usage

- The mirror release publishes per-arch bundles:
  - `drywintun-<version>-x64.zip`
  - `drywintun-<version>-arm64.zip`
  - `drywintun-<version>-x86.zip`
- During wheel builds, `scripts/fetch_wintun_release.py` downloads the matching bundle for each architecture and stages:
  - `py_tuntap_abi3/wintun/bin/<arch>/drywintun.dll`
  - `py_tuntap_abi3/wintun/bin/<arch>/drywintun.sys`
  - `py_tuntap_abi3/wintun/bin/<arch>/drywintun.inf`
  - `py_tuntap_abi3/wintun/bin/<arch>/drywintun.cat`
- `py_tuntap_abi3.__init__` adds that `<arch>` directory to the Windows DLL search path so `drywintun.dll` loads cleanly.
- In the `dryark/wintun` fork, driver install code still prefers embedded driver resources. If those resources are absent, it falls back to sidecar files from the same module directory, which is why `{sys,inf,cat}` are staged next to the DLL.

## License

- Project license file: `FC_LICENSE`
- Dry Ark LLC authored files include per-file Fair Coding License 1.0+ declarations.
