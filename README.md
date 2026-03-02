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

## Tunnel integration tests

- Fast/default test runs stay lightweight. Privileged tunnel tests are opt-in.
- Tunnel tests run only when `PYTUN_RUN_TUNNEL_TESTS=1` is set and tests are executed as root.
- Platform markers:
  - `tunnel_macos`
  - `tunnel_linux`

### macOS local (real tunnel path)

```bash
sudo env PYTUN_RUN_TUNNEL_TESTS=1 python3 -m pytest -q -m tunnel_macos tests/test_tunnel_macos.py
```

### Linux remote via SSH (`deploy-dryark@dryark.com`)

```bash
rsync -az --delete --exclude '.git' --exclude '.venv' --exclude '.venv-test' ./ deploy-dryark@dryark.com:/home/deploy-dryark/gitea/pytun-pmd3/
ssh deploy-dryark@dryark.com "set -euo pipefail; cd /home/deploy-dryark/gitea/pytun-pmd3; python3 -m venv .venv; ./.venv/bin/python -m pip install --upgrade pip setuptools wheel; SETUPTOOLS_SCM_PRETEND_VERSION_FOR_PY_TUNTAP_ABI3=0.0.dev0 ./.venv/bin/python -m pip install -e .; ./.venv/bin/python -m pip install pytest; sudo -n /usr/bin/env PYTUN_RUN_TUNNEL_TESTS=1 /home/deploy-dryark/gitea/pytun-pmd3/.venv/bin/python -m pytest -q -m tunnel_linux tests/test_tunnel_linux.py"
```

## License

- Project license file: `FC_LICENSE`
- Dry Ark LLC authored files include per-file Fair Coding License 1.0+ declarations.
