# edk2-cix

[![Release](https://github.com/radxa-pkg/edk2-cix/actions/workflows/release.yaml/badge.svg)](https://github.com/radxa-pkg/edk2-cix/actions/workflows/release.yaml)

## Build

1. `git clone -b main-monorepo-edk2 https://github.com/radxa-pkg/edk2-cix.git`
2. Open in [`devcontainer`](https://code.visualstudio.com/docs/devcontainers/containers)
3. `make deb`

For reproducible metadata on `main-monorepo-edk2`, the build uses the
merge-base with `main-monorepo-upstream-edk2` as its default source
identity. In the default `ARTEFACT_MODE=custom`, that commit identity
also supplies the default timestamp used for reproducible metadata.
You can inspect the resolved values with
`make -C src print-build-metadata`.

`main-monorepo-edk2` only supports Linux build hosts now. The old vendor
`WinBuildTool` tree and its Windows-only helper makefiles were removed from
this branch, so the supported local host environments are:

- Linux `x86_64`
- Linux `aarch64` / `arm64`

Before a longer build, run `make -C src preflight` to fail early if the
expected package-tool binaries or cross-compiler are missing.

For the firmware build itself, `make -C src` now defaults to EDK2 silent mode
so the transcript keeps the higher-level `Building ...` progress lines without
the full compiler-command firehose. Use `V=1` to restore the raw EDK2 command
output.

To capture a full build transcript plus a warning summary under `build-logs/`,
use `make buildbox-o6-log` or wrap any command with
`./scripts/capture_build_log.sh build-logs <command ...>`.

For exact replay of a published O6 image, `main-monorepo-edk2` can
also reuse an extracted FIP cert bundle via
`SIGNING_CERT_SOURCE_DIR=<path>`. The directory may contain either
the build-tree filenames `trusted_key_no.crt`, `nt_fw_cert.crt`, and
`nt_fw_key.crt` or the extracted FIP filenames `trusted-key-cert.bin`,
`nt-fw-cert.bin`, and `nt-fw-key-cert.bin`. When
`ARTEFACT_MODE=upstream` is combined with
`SIGNING_CERT_SOURCE_DIR=<path>`, that cert bundle is treated as
required replay input and the build now fails immediately if the
directory is missing or does not provide all three cert blobs.

Replay-compatible builds can also override the three historical
timestamp inputs independently:

- `BUILD_DATE=<iso8601>` for the displayed firmware build timestamp
- `SOURCE_DATE_EPOCH=<unix-seconds>` for compiler-provided `__DATE__`
  and `__TIME__` uses
- `PM_CONFIG_SOURCE_DATE_EPOCH=<unix-seconds>` for `csu_pm_config.bin`

When those explicit replay inputs are provided, the build no longer needs a
usable Git checkout in order to resolve source metadata. This keeps replay
wrappers and containerized builds quiet and deterministic even when they run
from a plain copied tree rather than a live Git worktree.

To extract those values automatically from an upstream O6 release artefact and
generate ready-to-run replay wrappers, use:

```bash
python3 src/scripts/replay_o6_release.py <edk2-cix_*.deb>
```

If you only have `cix_flash_all.bin`, pair it with `BuildOptions`
when available:

```bash
python3 src/scripts/replay_o6_release.py \
  cix_flash_all.bin \
  --build-options BuildOptions
```

The generated `rebuild-o6-docker.sh` wrapper recreates the upstream
`/workspaces/edk2-cix` path layout so that `ARTEFACT_MODE=upstream` can
reproduce the vendor release payloads byte-for-byte. By default it writes its
helper directory under the current system temp root and mounts that directory
into the build container automatically. If you want to stage those helper
files somewhere else, set `EDK2_CIX_HOST_TMPDIR` and, if needed,
`EDK2_CIX_CONTAINER_TMPDIR` when running the wrapper.

The first `make devcontainer_setup` run now drives `apt-get` in noninteractive
mode and suppresses recommends/suggests so the initial dependency bootstrap is
less noisy.

Native arm64 packaging is now viable on Debian Trixie-class userspaces, and
the current `O6` replay work has already shown that `amd64+trixie` and
`arm64+trixie` produce identical `cix_flash_*.bin`, `BuildOptions`, and
`csu_pm_config.bin` outputs. Native arm64 Bookworm is still too old for the
remaining vendored AARCH64 helpers because:

- `AARCH64/cix_package_tool` only needs `GLIBC_2.34`
- `AARCH64/cert_uefi_create_rsa` needs `GLIBC_2.38`

`fiptool` is now built from the vendored Arm Trusted Firmware-A source snapshot
under [src/tools/arm-trusted-firmware-fiptool](/Users/Stuart/src/edk2-cix/src/tools/arm-trusted-firmware-fiptool),
so the packaging path no longer depends on the old shipped `fiptool` binaries.
You can build it explicitly with:

```bash
make -C src host-fiptool
```

That source build depends on the normal OpenSSL development headers, so the
host dependency bootstrap now includes `libssl-dev`.

If you are specifically trying to recreate the published vendor release
payloads byte-for-byte, the currently validated replay path remains the amd64
Bookworm buildbox. If you want identical local outputs across `x86_64` and
`arm64`, pin both hosts to the same newer distro/toolchain generation such as
Trixie. That newer toolchain generation does change the compiled firmware
already at `SKY1_BL33_UEFI.fd`, even though `BuildOptions` remains identical
to the Bookworm replay baseline.

`bookworm` and `bookworm-backports` do not currently expose `gcc-13` or
`gcc-14` for the `aarch64-linux-gnu` cross toolchain in the default Debian
repositories, so the practical comparison matrix is currently:

- `bookworm + gcc-12 + binutils 2.40`
- `trixie + gcc-12 + binutils 2.44`
- `trixie + gcc-14 + binutils 2.44`

To check a local build against the stored exact-replay baseline without
re-running the whole release replay workflow, use:

```bash
make validate-firmware ARTEFACT_MODE=upstream
```

That compares the built `O6` outputs against the checked-in
`upstream-o6-1.2.1-bookworm` profile under
[validation/o6/expected-hashes.json](/Users/Stuart/src/edk2-cix/validation/o6/expected-hashes.json)
and writes a structural report under `build-validation/`.

When you use the top-level `firmware-build`, `firmware-stage`, `zip`, or
`targz` targets with `ARTEFACT_MODE=upstream`, that validation now runs
automatically and emits a loud warning if the local artefacts drift away from
the stored Bookworm replay baseline.

If you do not want a Debian package, the local Makefile extensions now provide
three direct payload workflows based on the same `O6` files that the `.deb`
ships:

```bash
make firmware-stage
make install
make zip
make targz
make validate-firmware
```

By default those targets work on the `O6` payload and:

- stage files under `dist/firmware/orion-o6/<version>/`
- install them under `/boot/efi/firmware/radxa/<version>/`
- write archives under `dist/`

The staged payload includes:

- top-level `startup.nsh` from `welcome.nsh`
- `BuildOptions`
- `cix_flash_all.bin`
- `cix_flash_ota.bin`
- `BurnImage.efi`
- `FlashUpdate.efi`
- `EnrollFromDefaultKeysApp.efi`
- `VariableInfo.efi`
- `Shell.efi`
- product-local `startup.nsh`

Override these knobs if needed:

- `FIRMWARE_BOARD=O6N`
- `FIRMWARE_PRODUCT=orion-o6n`
- `FIRMWARE_VERSION=<version>`
- `FIRMWARE_STAGE_ROOT=<path>`
- `FIRMWARE_INSTALL_ROOT=<path>`
- `FIRMWARE_ARCHIVE_ROOT=<path>`

The vendor `cix_package_tool` binary still emits ANSI colour escapes
unconditionally. The packaging rules now normalise that tool's output only, so
captured logs stay readable without globally rewriting unrelated command
output.

For host-side Python maintenance checks without entering the devcontainer, run:

```bash
python3 -W default -m compileall -q -f \
  src/edk2/BaseTools/Source/Python \
  src/edk2/BaseTools/Scripts \
  scripts
python3 -W default -m py_compile \
  src/edk2/ArmPlatformPkg/Scripts/Ds5/build_report.py
```

Release builds default to `ARTEFACT_MODE=custom`, which strips embedded
PE/COFF debug path records from the final firmware images. Use
`ARTEFACT_MODE=upstream` when you specifically want replay-compatible output
instead.
