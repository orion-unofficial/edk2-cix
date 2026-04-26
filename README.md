# edk2-cix

[![Release](https://github.com/radxa-pkg/edk2-cix/actions/workflows/release.yaml/badge.svg)](https://github.com/radxa-pkg/edk2-cix/actions/workflows/release.yaml)

This repo is a fork of the upstream Radxa Orion O6/O6N firmware tree. That
Radxa tree is itself built on CIX-published forks and payloads around
Tianocore `edk2`, Arm Trusted Firmware-A, and OP-TEE. This branch aims to
achieve two main things: preserve the upstream vendor build path,
including byte-identical rebuilds when the published replay inputs are
available, with a cleaner and more flexible build system; and let you build
custom firmware with targeted improvements that are intentionally kept off that
upstream vendor path.

Key custom additions on `main-monorepo` include:

- source-built replacements for several vendor helper binaries
- reproducible replay and validation tooling around the published O6/O6N
  releases
- opt-in firmware fixes for ACPI, PCIe, SMBIOS, PPTT, and related
  platform-description issues
- an opt-in experimental UEFI-settings overlay for additional board controls
- an opt-in curated `CIX_RELEASE=1.2` path that uses public CIX TF-A and
  OP-TEE sources for the early boot stages

## Build

1. `git clone -b main-monorepo-edk2 https://github.com/radxa-pkg/edk2-cix.git`
2. Choose one of the supported build paths below
3. Run the top-level `make` target you need

To see the common end-user targets first, run:

```bash
make help
```

For the build-variable reference, including which switches are custom-only,
which ones can be combined, and what they change on the board, see
[`docs/build-variables.md`](docs/build-variables.md).

To build the documentation locally, run `make docs-build` when `mdbook` is
already installed, or `devenv shell make docs-build` to use the repo's managed
docs toolchain. The rendered site is written to `book/html/`. Keep any
machine-specific `devenv` overrides, including optional Git hook installation,
in a local `devenv.local.nix` based on `devenv.local.nix.example`.

For GitHub Actions workflow changes, use the repo-local `act` wrapper before
pushing:

```bash
make gha-act-list
make gha-act-dry-run \
  ACT_WORKFLOW=.github/workflows/deterministic-replay.yaml \
  ACT_JOB=resolve-release
```

That wrapper bootstraps a pinned `act` binary under `.buildbox/tools/act/`,
keeps its cache under `.buildbox/act-cache/`, and applies the repo's default
runner mapping for `ubuntu-latest`. See [docs/build.md](docs/build.md) for the
full local-workflow testing notes and variables.

When a workflow touches generated paths such as `src/Build/` or `.buildbox/`,
prefer the clean helper so ignored local artefacts do not leak into the run:

```bash
make gha-act-run-clean \
  ACT_WORKFLOW=.github/workflows/custom-secure-boot.yaml \
  ACT_JOB=validate-custom-secure-boot \
  ACT_MATRIX=board:O6
```

### Without `devcontainer`

To build on a headless Linux host over SSH, you do not need VS Code or any IDE
integration.

On a supported Debian host:

- `x86_64`: prefer Debian `bookworm`
- `arm64` / `aarch64` on `main-monorepo`: use Debian `bookworm` by default,
  or Debian `trixie` for the newer distro/toolchain family

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

To keep the host cleaner and reuse a prepared build container, use the
buildbox helpers instead:

```bash
make buildbox-up
make buildbox-firmware-build
make buildbox-firmware-stage
make buildbox-deb
make buildbox-zip
make buildbox-targz
make buildbox-validate-firmware
make buildbox-capture-validation-profile
```

The local container helpers prefer a working Docker engine first when they can
clearly reach one that is not a Podman compatibility socket. Otherwise they
fall back to the platform default order: `docker` first on macOS and `podman`
first on Linux. If the preferred runtime is installed but not usable, they
automatically try the other one before failing. Set
`EDK2_CIX_CONTAINER_RUNTIME=docker` or `EDK2_CIX_CONTAINER_RUNTIME=podman` to
force a specific runtime.

By default the buildbox now follows the host architecture and uses the
Bookworm base image on both `x86_64` and `arm64` / `aarch64`. Override that
with `FIRMWARE_DISTRO=trixie`, `EDK2_CIX_BUILDBOX_PLATFORM`,
`EDK2_CIX_BUILDBOX_IMAGE`, `BUILDBOX_PLATFORM=...`, or `BUILDBOX_IMAGE=...`
when you need a specific container environment.

To select the distro generation explicitly:

- use `FIRMWARE_DISTRO=bookworm` for the default general buildbox environment
- use `FIRMWARE_DISTRO=trixie` for the Trixie general buildbox environment
- use `BUILDBOX_IMAGE=mcr.microsoft.com/devcontainers/base:...` (or
  `EDK2_CIX_BUILDBOX_IMAGE=...`) to override the image directly;
  when unset, the effective default is
  `mcr.microsoft.com/devcontainers/base:${FIRMWARE_DISTRO}`

The `buildbox-*` targets keep their host-side scratch space under
`.buildbox/`, write staged/archive outputs under `dist/`, and copy Debian
package artefacts into `dist/deb/`.

For non-buildbox workflows, the base OS is whichever environment you are
already building in. Host-native builds therefore use the host distro, and the
checked-in devcontainer remains pinned by
[`/.devcontainer/devcontainer.json`](.devcontainer/devcontainer.json).

The firmware-oriented `buildbox-*` targets install the slimmer firmware
dependency profile by default; `buildbox-deb` switches the same reusable
container to the fuller packaging profile when it needs Debian packaging
tools. You can also invoke them from another working directory with
`make -C /path/to/checkout buildbox-...`.

### With `devcontainer`

To use the full containerized developer environment, open the repo in a
[`devcontainer`](https://code.visualstudio.com/docs/devcontainers/containers)
and then run:

```bash
make deb
```

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

The short `make help-vars` output is meant to be a quick reminder. For the
more detailed “what does this actually change?” explanation of variables such
as `ARTEFACT_MODE`, `ENABLE_FIRMWARE_FIXES`, `ENABLE_EXPERIMENTAL_UEFI_SETTINGS`,
`ENABLE_CORE_ORDER`, `CIX_RELEASE`, `UART3_ENABLE`, and the `DEBUG_*` switches,
use [`docs/build-variables.md`](docs/build-variables.md).

For the firmware build itself, the repo now defaults the underlying EDK2 build
to silent mode, so the transcript keeps the higher-level `Building ...`
progress lines without the full compiler-command firehose. That applies both to
direct `make -C src ...` invocations and to the top-level `make` targets that
delegate into `src`. Use `V=1` on the `make` command line to restore the raw
EDK2 command output.

To capture a full build transcript plus a warning summary under `build-logs/`,
use `make buildbox-firmware-log` or wrap any command with
`./scripts/capture_build_log.sh build-logs <command ...>`.

For exact replay of a published O6 or O6N image,
`main-monorepo-edk2` can also reuse an extracted FIP cert bundle via
`SIGNING_CERT_SOURCE_DIR=<path>`. The directory may contain either
the build-tree filenames `trusted_key_no.crt`, `nt_fw_cert.crt`, and
`nt_fw_key.crt` or the extracted FIP filenames `trusted-key-cert.bin`,
`nt-fw-cert.bin`, and `nt-fw-key-cert.bin`. When
`ARTEFACT_MODE=upstream` is combined with
`SIGNING_CERT_SOURCE_DIR=<path>`, that cert bundle is treated as
required replay input and the build now fails immediately if the
directory is missing or does not provide all three cert blobs.

Replay-compatible builds can also override the three historical timestamp
inputs independently:

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
into the build container automatically. To stage those helper files somewhere
else, set `EDK2_CIX_HOST_TMPDIR` and, if needed,
`EDK2_CIX_CONTAINER_TMPDIR` when running the wrapper.

For the common qualification/replay flow, you can drive the same process from
the top-level Makefile instead:

```bash
make deterministic-replay \
  REPLAY_INPUT=/path/to/edk2-cix_1.2.1_all.deb
```

That target defaults to `FIRMWARE_BOARD=O6` and
`FIRMWARE_DISTRO=bookworm`, seeds or reuses a cached replay-input directory
under `.buildbox/replay/<profile>/`, rebuilds in the matching buildbox image,
and then runs strict validation against the checked-in replay profile. When the
input is the published `1.2.1` release plus its extracted cert bundle, this is
the qualification path that proves the Bookworm build can still reproduce the
published payloads byte-for-byte.

You can also switch to `FIRMWARE_DISTRO=trixie` for a same-input Trixie
replay. In that mode the goal is matching `amd64` and `arm64` outputs
against the same cert bundle and injected timestamps, not comparison with a
published upstream release.

If you already populated `.buildbox/replay/<profile>/` once, later reruns can
omit `REPLAY_INPUT=...` and will reuse the cached `replay.env` plus cert
bundle. When the replay input is only `cix_flash_all.bin`, also pass
`REPLAY_BUILD_OPTIONS=/path/to/BuildOptions` when available so the helper can
recover `BUILD_DATE`.

Without explicit replay inputs, both `ARTEFACT_MODE=custom` and ordinary
`ARTEFACT_MODE=upstream` builds resolve `SOURCE_COMMIT_HASH`,
`SOURCE_DATE_EPOCH`, `PM_CONFIG_SOURCE_DATE_EPOCH`, and `BUILD_DATE` from the
mapped upstream source commit in Git history. On the custom path, that keeps
the displayed build metadata tied to the upstream tag or commit being built
rather than the local overlay commit.

Despite the retained filename, the local helper scripts prefer a working Docker
engine first when they can positively distinguish it from Podman. Otherwise
they fall back to `docker` first on macOS and `podman` first on Linux, then
try the other runtime if needed. The generated wrapper still pins the replay
buildbox back to the validated amd64 Bookworm environment explicitly.

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

To pick the buildbox distro family explicitly, set either:

- `FIRMWARE_DISTRO=bookworm`
- `FIRMWARE_DISTRO=trixie`

To use a specific image instead, `BUILDBOX_IMAGE=...` overrides
`FIRMWARE_DISTRO`. When unset, the default effective image is
`mcr.microsoft.com/devcontainers/base:${FIRMWARE_DISTRO}`.

Native `arm64` / `aarch64` work no longer depends on the old closed-source
`cert_uefi_create_rsa` helper. The local build now compiles both that tool and
`fiptool` from source, and the packaging step runs the in-tree source
`cix_package_tool` implementation too.

The source replacements now live under [src/tools](src/tools):

- [src/tools/arm-trusted-firmware-fiptool](src/tools/arm-trusted-firmware-fiptool)
- [src/tools/cert_uefi_create_rsa](src/tools/cert_uefi_create_rsa)
- [src/tools/cix_package_tool](src/tools/cix_package_tool)
- [src/tools/cix_regen_trusted_key_cert](src/tools/cix_regen_trusted_key_cert)

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

For reproducibility qualification, the checked-in validation profiles serve two
different purposes:

- `upstream-1.2.1-bookworm`
  - shared validation profile for the published `1.2.1` upstream release
    builds
  - select `FIRMWARE_BOARD=O6` or `FIRMWARE_BOARD=O6N` to validate the matching
    board baseline under the same profile name
  - expected to match only when the build reuses the extracted release cert
    bundle and the corresponding replay timestamps
- `upstream-1.2.1-trixie`
  - same-input Trixie reproducibility profile
  - currently records the checked O6 Trixie baseline under the shared profile
    naming scheme
  - used to confirm matching `amd64` and `arm64` Trixie builds on
    `main-monorepo`
  - not a published upstream-release baseline

Freshly generated certs are compatible but not byte-identical because they
inherently carry signing-time entropy. On `arm64` / `aarch64`, you can still
force the amd64 replay path with `BUILDBOX_PLATFORM=linux/amd64` or
`EDK2_CIX_BUILDBOX_PLATFORM=linux/amd64` once your container runtime has x86_64
emulation configured.

To check a build against the stored validation profile without re-running the
whole replay workflow, use:

```bash
make validate-firmware ARTEFACT_MODE=upstream
```

That compares the built outputs against the checked-in validation profile under
[validation/expected-hashes.json](validation/expected-hashes.json) and writes a
structural report under `build-validation/`.

For the published O6N release baseline, select the shared Bookworm profile and
the `O6N` board:

```bash
make validate-firmware \
  ARTEFACT_MODE=upstream \
  FIRMWARE_BOARD=O6N \
  FIRMWARE_VALIDATION_PROFILE=upstream-1.2.1-bookworm
```

The same file also carries the `upstream-1.2.1-trixie` reproducibility profile,
intended for matching amd64 or arm64 Trixie O6 replays on
`main-monorepo` when they reuse the same cert bundle and replay timestamps:

```bash
make validate-firmware \
  ARTEFACT_MODE=upstream \
  FIRMWARE_VALIDATION_PROFILE=upstream-1.2.1-trixie
```

Use the shared `upstream-<version>-<distro>` form for replay-profile names on
this branch.

For deeper offline regression checks against the currently checked-in Bookworm
replay baselines, this branch also includes:

```bash
make check-offline-audit-baselines
make audit-final-image-manifest ARTEFACT_MODE=upstream
make audit-acpi-regression ARTEFACT_MODE=upstream
make audit-bundle ARTEFACT_MODE=upstream
make buildbox-audit-bundle ARTEFACT_MODE=upstream FIRMWARE_BOARD=O6
```

Those checks write JSON reports under `build-validation/`.
The checked-in offline baseline files currently cover the shared
`upstream-1.2.1-bookworm` profile for both `O6` and `O6N`; use the audit
scripts' `--emit-baseline` mode later to extend that coverage.

- the final-image manifest audit baselines the final BL33 FV / FFS / section
  composition
- the ACPI audit baselines emitted AML tables plus rerun `iasl` warning and
  remark families
- `make check-offline-audit-baselines` verifies the checked-in offline audit
  baseline JSON structure before those audits run
- `make refresh-offline-audit-baselines ARTEFACT_MODE=upstream` refreshes both
  offline audit baseline files together from the already-built selected board
  tree for the active `FIRMWARE_VALIDATION_PROFILE`
- the `audit-bundle` target combines the qualification checks plus the payload
  metadata self-test, and optionally also runs the metadata branch consistency
  checker if `MONOREPO_META_ROOT=/path/to/main-monorepo-meta` is set

The deterministic replay workflow now also uploads the generated
`build-validation/*.json` files as CI artefacts and emits a short Markdown job
summary listing the validation and audit report statuses.

For a maintainer-driven full qualification pass on GitHub Actions, the tree now
also includes `.github/workflows/maintainer-audit-bundle.yaml`. That manual
workflow runs the `buildbox-audit-bundle` target for a selected board/profile,
checks metadata consistency against `main-monorepo-meta`, uploads the generated
`build-validation/*.json` reports, and writes the same short Markdown summary
to the job page.

To snapshot the current build into a fresh profile JSON for later review or to
seed a new validation baseline under a profile name you choose, use:

```bash
make capture-validation-profile \
  FIRMWARE_VALIDATION_PROFILE=<new-profile-name>
```

Ordinary `ARTEFACT_MODE=upstream` builds with freshly generated certs or
non-replay timestamps are not expected to match either stored profile.
Top-level `firmware-build`, `firmware-stage`, `zip`, and `targz` therefore no
longer run validation automatically by default. Use `make validate-firmware`,
`make validate-firmware-strict`, or `make deterministic-replay` explicitly when
you want qualification checks, or set `FIRMWARE_VALIDATE_ON_BUILD=true` to
restore the advisory post-build validation step.

If you do not want a Debian package, the local Makefile extensions now provide
several direct payload targets based on the same board-specific files that the
`.deb` ships:

```bash
make firmware-stage
make install
make zip
make targz
make validate-firmware
```

By default those targets operate on the `O6` payload. Set
`FIRMWARE_BOARD=O6N` to switch products; the default `O6` paths are:

- stage files under `dist/firmware/orion-o6/<version>/`
- install them under `/boot/efi/edk2/radxa/orion-o6/<version>/`
- `make install` is intended for a live system with a writable EFI system
  partition
- Debian package builds do not use `make install`; they collect files from the
  build tree via `dh_install`
- write archives under `dist/`

The `.zip` and `.tar.gz` exports keep the payload under
`orion-o6/<version>/` inside the archive, so the archive contents mirror the
same product/version layout as the staged payload.

When you use `make buildbox-firmware-stage`, the staged payload is already
written back into the host checkout at the same default
`dist/firmware/orion-o6/<version>/` path, because the buildbox bind-mounts the
repo root. There is no separate `podman cp` or `docker cp` step.

The staged payload is self-contained inside that `<version>/` directory and
includes:

- `BuildOptions`
- `cix_flash_all.bin`
- `cix_flash_ota.bin`
- `BurnImage.efi`
- `FlashUpdate.efi`
- `EnrollFromDefaultKeysApp.efi`
- `VariableInfo.efi`
- `Shell.efi`
- `startup.nsh`
- `tools/LoadOpRom.efi` on `ARTEFACT_MODE=custom` `O6` exports

To deploy that staged payload manually onto a target machine or removable FAT
volume, copy `dist/firmware/orion-o6/` into `edk2/radxa/` on the EFI System
Partition. That gives you:

- `edk2/radxa/orion-o6/<version>/BuildOptions`
- `edk2/radxa/orion-o6/<version>/cix_flash_all.bin`
- `edk2/radxa/orion-o6/<version>/cix_flash_ota.bin`
- `edk2/radxa/orion-o6/<version>/BurnImage.efi`
- `edk2/radxa/orion-o6/<version>/FlashUpdate.efi`
- `edk2/radxa/orion-o6/<version>/EnrollFromDefaultKeysApp.efi`
- `edk2/radxa/orion-o6/<version>/VariableInfo.efi`
- `edk2/radxa/orion-o6/<version>/Shell.efi`
- `edk2/radxa/orion-o6/<version>/startup.nsh`
- `edk2/radxa/orion-o6/<version>/tools/LoadOpRom.efi` on custom `O6` exports

From the UEFI Shell, run
`fs0:\edk2\radxa\orion-o6\<version>\startup.nsh` directly to flash that
payload.

For custom-path builds, `O6N` now omits `X86EmulatorDxe` from the firmware
image, and custom `O6` non-`deb` exports scrub `tools/LoadOpRom.efi` with
`GenFw --zero` before staging it for manual OpROM debugging.

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
