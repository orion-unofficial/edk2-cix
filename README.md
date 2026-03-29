# edk2-cix

[![Release](https://github.com/radxa-pkg/edk2-cix/actions/workflows/release.yaml/badge.svg)](https://github.com/radxa-pkg/edk2-cix/actions/workflows/release.yaml)

## Build

1. `git clone -b main-monorepo https://github.com/radxa-pkg/edk2-cix.git`
2. Choose one of the supported build paths below
3. Run the top-level `make` target you need

To see the common end-user targets first, run:

```bash
make help
```

### Without `devcontainer`

If you are building on a headless Linux host over SSH, you do not need VS Code
or any IDE integration.

On a supported Debian host:

- `x86_64`: prefer Debian `bookworm`
- `arm64` / `aarch64`: use Debian `trixie`

For exact upstream replay on `arm64` / `aarch64`, Debian `bookworm` is also
confirmed when you reuse the extracted cert bundle instead of generating fresh
vendor cert artefacts locally.

To install the required host packages directly on the machine, run:

```bash
make firmware_build_dep
make devcontainer_setup
```

`make firmware_build_dep` installs the slimmer direct-firmware dependency
profile. `make devcontainer_setup` installs the fuller packaging-capable
dependency profile. Both are just the repo's dependency bootstrap, so they
work fine outside a devcontainer.

Then build either the Debian package or the direct firmware payloads:

```bash
make deb
make firmware-build
make firmware-stage
make zip
make targz
```

If you would rather keep the host cleaner and reuse a prepared build container,
use the buildbox helpers instead:

```bash
make buildbox-up
make buildbox-firmware-build
make buildbox-firmware-stage
make buildbox-o6
make buildbox-deb
make buildbox-zip
make buildbox-targz
make buildbox-validate-firmware
make buildbox-capture-validation-profile
```

The local container helpers prefer `podman` on Linux and `docker` on macOS.
If the preferred runtime is installed but not usable, they automatically try
the other one before failing. Set `EDK2_CIX_CONTAINER_RUNTIME=docker` or
`EDK2_CIX_CONTAINER_RUNTIME=podman` to force a specific runtime.

By default the buildbox now follows the host architecture: `linux/amd64` with
the Bookworm base image on `x86_64`, and `linux/arm64` with the Trixie base
image on `arm64` / `aarch64`. Override that with
`EDK2_CIX_BUILDBOX_PLATFORM`, `EDK2_CIX_BUILDBOX_IMAGE`,
`BUILDBOX_PLATFORM=...`, or `BUILDBOX_IMAGE=...` when you need a specific
container environment.

The `buildbox-*` targets keep their host-side scratch space under
`.buildbox/`, write staged/archive outputs under `dist/`, and copy Debian
package artefacts into `dist/deb/`.

The firmware-oriented `buildbox-*` targets install the slimmer firmware
dependency profile by default; `buildbox-deb` switches the same reusable
container to the fuller packaging profile when it needs Debian packaging
tools. You can also invoke them from another working directory with
`make -C /path/to/checkout buildbox-...`.

### With `devcontainer`

If you do want the full containerized developer environment, open the repo in a
[`devcontainer`](https://code.visualstudio.com/docs/devcontainers/containers)
and then run:

```bash
make deb
```

`main-monorepo` only supports Linux build hosts now. The old vendor
`WinBuildTool` tree and its Windows-only helper makefiles were removed from
this branch, so the supported local host environments are:

- Linux `x86_64`
- Linux `aarch64` / `arm64`

Before a longer build, run `make -C src preflight` to fail early if the
expected package-tool binaries or cross-compiler are missing.

For the firmware build itself, the repo now defaults the underlying EDK2 build
to silent mode, so the transcript keeps the higher-level `Building ...`
progress lines without the full compiler-command firehose. That applies both to
direct `make -C src ...` invocations and to the top-level `make` targets that
delegate into `src`. Use `V=1` on the `make` command line to restore the raw
EDK2 command output.

To capture a full build transcript plus a warning summary under `build-logs/`,
use `make buildbox-o6-log` or wrap any command with
`./scripts/capture_build_log.sh build-logs <command ...>`.

For reproducible metadata on `main-monorepo`, the build uses the nearest
mapped upstream `main` commit as its default source identity. In the default
`ARTEFACT_MODE=custom`, that commit identity also supplies the default
timestamp used for reproducible metadata. You can inspect the resolved values
with `make -C src print-build-metadata`.

For exact replay of a published O6 image, `main-monorepo` can also reuse an
extracted FIP cert bundle via `SIGNING_CERT_SOURCE_DIR=<path>`. The directory
may contain either the build-tree filenames `trusted_key_no.crt`,
`nt_fw_cert.crt`, and `nt_fw_key.crt` or the extracted FIP filenames
`trusted-key-cert.bin`, `nt-fw-cert.bin`, and `nt-fw-key-cert.bin`.

When `ARTEFACT_MODE=upstream` is combined with
`SIGNING_CERT_SOURCE_DIR=<path>`, that cert bundle is treated as required
replay input and the build now fails immediately if the directory is missing
or does not provide all three cert blobs.

Replay-compatible builds can also override the three historical timestamp
inputs independently:

- `BUILD_DATE=<iso8601>` for the displayed firmware build timestamp
- `SOURCE_DATE_EPOCH=<unix-seconds>` for compiler-provided `__DATE__` and
  `__TIME__` uses
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

If you only have `cix_flash_all.bin`, pair it with `BuildOptions` when
available:

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

Despite the retained filename, the local helper scripts prefer `podman` on
Linux and `docker` on macOS, then fall back to the other runtime if needed.
The generated wrapper now also pins the buildbox back to the validated amd64
Bookworm replay environment explicitly.

The first `make devcontainer_setup` run now drives `apt-get` in noninteractive
mode and suppresses recommends/suggests so the initial dependency bootstrap is
less noisy.

The buildbox helpers use the slimmer firmware dependency profile by default
and only switch to the fuller packaging profile for `buildbox-deb`.

The firmware build also records the userspace and toolchain context used for
the EDK2 `BaseTools/Source/C` helpers. If that context changes, the next build
rebuilds those helper binaries automatically instead of reusing stale
host-built copies.

The preferred distribution for local `x86_64` builds remains Debian
`bookworm`.

Native `arm64` / `aarch64` builds require Debian `trixie`, because the
remaining closed-source vendor AARCH64 helpers cannot run on Bookworm. In
particular:

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
Bookworm buildbox. On `arm64` / `aarch64`, force that path with
`BUILDBOX_PLATFORM=linux/amd64` or `EDK2_CIX_BUILDBOX_PLATFORM=linux/amd64`
once your container runtime has x86_64 emulation configured.

`bookworm` and `bookworm-backports` do not currently expose `gcc-13` or
`gcc-14` for the `aarch64-linux-gnu` cross toolchain in the default Debian
repositories.

To check a build against the stored exact-replay baseline without
re-running the whole release replay workflow, use:

```bash
make validate-firmware ARTEFACT_MODE=upstream
```

That compares the built `O6` outputs against the checked-in
`upstream-o6-1.2.1-bookworm` profile under
[validation/o6/expected-hashes.json](/Users/Stuart/src/edk2-cix/validation/o6/expected-hashes.json)
and writes a structural report under `build-validation/`.

For the current Trixie-era local build family, the same file also carries a
`modern-o6-trixie-structural` profile. That profile checks stable metadata and
EFI section layout without requiring byte-identical output:

```bash
make validate-firmware FIRMWARE_VALIDATION_PROFILE=modern-o6-trixie-structural
```

To snapshot the current build into a fresh profile JSON for later review or to
seed a new validation baseline, use:

```bash
make capture-validation-profile \
  FIRMWARE_VALIDATION_PROFILE=modern-o6-trixie-structural
```

When you use the top-level `firmware-build`, `firmware-stage`, `zip`, or
`targz` targets with `ARTEFACT_MODE=upstream`, that validation now runs
automatically and emits a loud warning if the local artefacts drift away from
the stored Bookworm replay baseline.

If you do not want a Debian package, the local Makefile extensions now provide
several direct payload targets based on the same `O6` files that the `.deb`
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
- `make install` is intended for a live system with a writable EFI system
  partition
- Debian package builds do not use `make install`; they collect files from the
  build tree via `dh_install`
- write archives under `dist/`

The `.zip` and `.tar.gz` exports keep the payload under
`orion-o6/<version>/` inside the archive, so the archive contents mirror the
same product/version layout as the staged payload.

When you use `make buildbox-firmware-stage`, the staged payload is already
written back into the host checkout at the same `dist/firmware/orion-o6/<version>/`
path, because the buildbox bind-mounts the repo root. There is no separate
`podman cp` or `docker cp` step.

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

To deploy that staged payload manually onto a target machine or removable FAT
volume, copy the contents of `dist/firmware/orion-o6/<version>/` into
`edk2/radxa/` on the EFI System Partition. That gives you:

- `edk2/radxa/startup.nsh` as the top-level launcher
- `edk2/radxa/orion-o6/` with the board-specific flash binaries and `.efi`
  helpers

From the UEFI Shell, run `startup.nsh` to list available products, or go
straight to `orion-o6/startup.nsh` to flash that payload directly.

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
