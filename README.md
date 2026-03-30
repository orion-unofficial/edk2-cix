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

To build the documentation locally, run `make docs-build` when `mdbook` is
already installed, or `devenv shell make docs-build` when you want the repo's
managed docs toolchain. The rendered site is written to `book/html/`.

### Without `devcontainer`

If you are building on a headless Linux host over SSH, you do not need VS Code
or any IDE integration.

On a supported Debian host:

- `x86_64`: prefer Debian `bookworm`
- `arm64` / `aarch64` on `main-monorepo`: use Debian `bookworm` by default,
  or Debian `trixie` when you want the newer distro/toolchain family

The untouched upstream repo contents still need Debian `trixie` for native
`arm64` / `aarch64` builds because they shipped closed-source helper binaries.
`main-monorepo` replaces those helpers with source implementations, so native
`arm64` / `aarch64` builds can now use the same default Debian `bookworm` base
as `x86_64` builds.

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

By default the buildbox now follows the host architecture and uses the
Bookworm base image on both `x86_64` and `arm64` / `aarch64`. Override that
with `FIRMWARE_DISTRO=trixie`, `EDK2_CIX_BUILDBOX_PLATFORM`,
`EDK2_CIX_BUILDBOX_IMAGE`, `BUILDBOX_PLATFORM=...`, or `BUILDBOX_IMAGE=...`
when you need a specific container environment.

To select the distro generation explicitly:

- use `FIRMWARE_DISTRO=bookworm` for the default general buildbox environment
- use `FIRMWARE_DISTRO=trixie` for the Trixie general buildbox environment
- use `BUILDBOX_IMAGE=mcr.microsoft.com/devcontainers/base:...` (or
  `EDK2_CIX_BUILDBOX_IMAGE=...`) when you want to override the image directly;
  when unset, the effective default is
  `mcr.microsoft.com/devcontainers/base:${FIRMWARE_DISTRO}`

The `buildbox-*` targets keep their host-side scratch space under
`.buildbox/`, write staged/archive outputs under `dist/`, and copy Debian
package artefacts into `dist/deb/`.

For non-buildbox workflows, the base OS is whichever environment you are
already building in. Host-native builds therefore use the host distro, and the
checked-in devcontainer remains pinned by
[`/.devcontainer/devcontainer.json`](/Users/Stuart/src/edk2-cix/.devcontainer/devcontainer.json).

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

For the common case, you can drive the same exact-replay flow from the
top-level Makefile instead:

```bash
make deterministic-replay \
  REPLAY_INPUT=/path/to/edk2-cix_1.2.1_all.deb
```

That target defaults to `FIRMWARE_BOARD=O6` and `FIRMWARE_DISTRO=bookworm`, seeds
or reuses a cached replay-input directory under
`.buildbox/replay/<profile>/`, rebuilds in the matching buildbox image, and
then runs strict validation against the checked-in exact replay profile.

You can switch to the validated Trixie replay family with:

```bash
make deterministic-replay \
  FIRMWARE_DISTRO=trixie \
  REPLAY_INPUT=/path/to/edk2-cix_1.2.1_all.deb
```

If you already populated `.buildbox/replay/<profile>/` once, later reruns can
omit `REPLAY_INPUT=...` and will reuse the cached `replay.env` plus cert
bundle. When the replay input is only `cix_flash_all.bin`, also pass
`REPLAY_BUILD_OPTIONS=/path/to/BuildOptions` when available so the helper can
recover `BUILD_DATE`.

Despite the retained filename, the local helper scripts prefer `podman` on
Linux and `docker` on macOS, then fall back to the other runtime if needed.
The generated wrapper still pins the replay buildbox back to the validated
amd64 Bookworm environment explicitly.

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

In the original upstream tree, native `arm64` / `aarch64` builds needed the
`trixie` buildbox because the shipped closed-source helpers were not portable
enough for the Bookworm arm64 path. On `main-monorepo`, those helpers are now
reimplemented from source, so both `amd64` and native `arm64` can use the
same default `bookworm` buildbox for exact replay and local builds.

If you want to pick the buildbox distro family explicitly, set either:

- `FIRMWARE_DISTRO=bookworm`
- `FIRMWARE_DISTRO=trixie`

If you need a specific image instead, `BUILDBOX_IMAGE=...` overrides
`FIRMWARE_DISTRO`. When unset, the default effective image is
`mcr.microsoft.com/devcontainers/base:${FIRMWARE_DISTRO}`.

Native `arm64` / `aarch64` work no longer depends on the old closed-source
`cert_uefi_create_rsa` helper. The local build now compiles both that tool and
`fiptool` from source, and the packaging step runs the in-tree source
`cix_package_tool` implementation too.

The maintained helper sources now live under [src/tools](/Users/Stuart/src/edk2-cix/src/tools):

- [src/tools/arm-trusted-firmware-fiptool](/Users/Stuart/src/edk2-cix/src/tools/arm-trusted-firmware-fiptool)
- [src/tools/cert_uefi_create_rsa](/Users/Stuart/src/edk2-cix/src/tools/cert_uefi_create_rsa)
- [src/tools/cix_package_tool](/Users/Stuart/src/edk2-cix/src/tools/cix_package_tool)
- [src/tools/cix_regen_trusted_key_cert](/Users/Stuart/src/edk2-cix/src/tools/cix_regen_trusted_key_cert)

That keeps the build and replay paths off the old shipped opaque binaries.
You can build them explicitly with:

```bash
make -C src host-fiptool
make -C src host-cert-uefi-create-rsa
make -C src host-cix-regen-trusted-key-cert
make -C src host-cix-package-tool
```

That source build depends on the normal OpenSSL development headers, so the
host dependency bootstrap now includes `libssl-dev`.

If you are specifically trying to recreate the published vendor release
payloads byte-for-byte, the validated exact replay profiles are:

- `upstream-o6-1.2.1-bookworm`
- `upstream-o6-1.2.1-trixie`

Both profiles are confirmed on `amd64` and native `arm64` when the extracted
cert bundle is reused. Freshly generated certs are compatible but not
byte-identical because they inherently carry signing-time entropy. On `arm64`
/ `aarch64`, you can still force the amd64 replay path with
`BUILDBOX_PLATFORM=linux/amd64` or `EDK2_CIX_BUILDBOX_PLATFORM=linux/amd64`
once your container runtime has x86_64 emulation configured.

To check a build against the stored exact-replay baseline without
re-running the whole release replay workflow, use:

```bash
make validate-firmware ARTEFACT_MODE=upstream
```

That compares the built `O6` outputs against the checked-in
`upstream-o6-1.2.1-bookworm` profile under
[validation/expected-hashes.json](/Users/Stuart/src/edk2-cix/validation/expected-hashes.json)
and writes a structural report under `build-validation/`.

The same file also carries an exact `upstream-o6-1.2.1-trixie` profile,
generated from an amd64 Trixie build of upstream `main` and intended for
matching amd64 or arm64 Trixie replays on `main-monorepo`:

```bash
make validate-firmware \
  ARTEFACT_MODE=upstream \
  FIRMWARE_VALIDATION_PROFILE=upstream-o6-1.2.1-trixie
```

To snapshot the current build into a fresh profile JSON for later review or to
seed a new validation baseline under a profile name you choose, use:

```bash
make capture-validation-profile \
  FIRMWARE_VALIDATION_PROFILE=<new-profile-name>
```

When you use the top-level `firmware-build`, `firmware-stage`, `zip`, or
`targz` targets with `ARTEFACT_MODE=upstream`, that validation now runs
automatically and emits a loud warning if the local artefacts drift away from
the active exact replay profile.

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
instead. When `ARTEFACT_MODE=custom`, the build also prepends matching package
roots under `custom/overlay/` to `PACKAGES_PATH`, so source-level custom
overrides live outside the imported upstream trees.
