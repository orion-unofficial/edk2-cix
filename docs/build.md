# Build

You can build this repo either directly on a supported Debian host or inside a
containerized environment.

To start with a quick overview of the common top-level workflows, run:

```bash
make help
```

For lower-level firmware targets, use:

```bash
make -C src help
```

For the fuller explanation of the user-facing build variables and their side
effects, see [`build-variables.md`](build-variables.md).

For documentation builds, run `make docs-build` when `mdbook` is already
available, or `devenv shell make docs-build` to use the repo's managed docs
toolchain. The generated site is written to `book/html/`.

## Test GitHub Actions locally

For workflow changes, prefer testing them locally with the repo-managed `act`
wrapper before pushing:

```bash
make gha-act-list
make gha-act-dry-run \
  ACT_WORKFLOW=.github/workflows/deterministic-replay.yaml \
  ACT_JOB=resolve-release
make gha-act-run \
  ACT_WORKFLOW=.github/workflows/deterministic-replay.yaml \
  ACT_JOB=resolve-release
```

The wrapper lives at `scripts/run_github_actions_with_act.sh`. It bootstraps a
pinned `act` binary under `.buildbox/tools/act/`, keeps its cache under
`.buildbox/act-cache/`, maps `ubuntu-latest` to
`catthehacker/ubuntu:act-latest`, and defaults the act runner architecture to
`linux/amd64` so workflow behaviour stays closer to hosted GitHub Actions.

For workflows that build under ignored paths like `src/Build/` or `.buildbox/`,
use the clean helper instead:

```bash
make gha-act-run-clean \
  ACT_WORKFLOW=.github/workflows/custom-secure-boot.yaml \
  ACT_JOB=validate-custom-secure-boot \
  ACT_MATRIX=board:O6N
```

That helper clones a scratch checkout, overlays the current tracked and
untracked non-ignored working tree changes, and runs `act` there so stale
ignored artefacts from your main checkout cannot skew the result. Set
`ACT_CLEAN_KEEP=1` to preserve that scratch checkout for debugging.

Common variables:

- `ACT_WORKFLOW=.github/workflows/<file>.yaml`
- `ACT_EVENT=workflow_dispatch|push|pull_request`
- `ACT_JOB=<job-id>`
- `ACT_MATRIX=<name:value>` for a single matrix leg such as `board:O6`
- `ACT_SECRET_FILE=/path/to/secrets.env` when a local run needs secrets
- `ACT_EXTRA_ARGS='...'` for any extra raw act flags
- `ACT_CLEAN_KEEP=1` to keep the temporary checkout used by `gha-act-run-clean`

To pass the runner image or container architecture yourself, call
`scripts/run_github_actions_with_act.sh --no-defaults ...` directly.

The first run downloads the pinned `act` release plus the selected runner
image, so it is expected to take longer. In sandboxed agent environments,
`act` may also need permission to talk to Docker and bind its local helper
networking before the run can start.

## Supported host environments

- Debian `bookworm` on `x86_64` for the preferred upstream vendor path,
  including byte-identical replay when you provide the published replay inputs
- Debian `bookworm` on `arm64` / `aarch64` for the default `main-monorepo`
  build path, including exact replay when you reuse the extracted cert bundle
- Debian `trixie` on `arm64` / `aarch64` for the newer distro / toolchain
  family on `main-monorepo`

The untouched upstream repo contents still need Debian `trixie` for native
`arm64` / `aarch64` builds because they shipped closed-source helper binaries.
`main-monorepo` replaces those helpers with source implementations, so native
`arm64` / `aarch64` can now use the same default Debian `bookworm` base as
`x86_64`.

We still support `devcontainer`, but it is optional.

## Build directly on a host

For a headless SSH session or any other non-IDE workflow, bootstrap the build
dependencies directly on the machine:

```bash
make firmware_build_dep
make devcontainer_setup
```

Use `make firmware_build_dep` when you only need direct firmware builds. Use
`make devcontainer_setup` when you also want Debian packaging and the broader
maintainer tool set.

Then build whatever output you need:

```bash
make deb
make firmware-build
make firmware-stage
make zip
make targz
```

To reuse a warmed build container instead of installing the dependencies onto
the host, use:

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
By default the buildbox follows the host architecture: `linux/amd64` with the
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
The firmware-oriented `buildbox-*` targets install the slimmer firmware
dependency profile inside the reusable container; `buildbox-deb` switches that
same container to the fuller packaging profile when needed.

For non-buildbox workflows, the base OS is whichever environment you are
already building in. Host-native builds therefore use the host distro, and the
checked-in devcontainer remains pinned by
[`/.devcontainer/devcontainer.json`](https://github.com/radxa-pkg/edk2-cix/blob/main-monorepo/.devcontainer/devcontainer.json).

## Build inside a devcontainer

We still use devcontainers to keep one known-good amd64 environment around.

To build all supported EDK2 variants inside that environment, run:

```bash
make deb
```

`main-monorepo-edk2` only supports Linux build hosts now. The old vendor
`WinBuildTool` tree and its Windows-only helper makefiles were removed from
this branch, so the supported local host environments are:

- Linux `x86_64`
- Linux `aarch64` / `arm64`

Before a longer build, run `make -C src preflight` to fail early if the
expected package-tool binaries, source directories, or cross-compiler are
missing.

Set `FIRMWARE_TARGET=DEBUG` on the `make -C src ...` command line to build
debug artefacts instead of the default release output tree under
`Build/.../RELEASE_GCC5/`.

For the detailed explanation of how `ARTEFACT_MODE`,
`ENABLE_FIRMWARE_FIXES`, `ENABLE_EXPERIMENTAL_UEFI_SETTINGS`,
`ENABLE_CORE_ORDER`, `CIX_RELEASE`, `UART3_ENABLE`, and the `DEBUG_*`
variables interact, see [`build-variables.md`](build-variables.md).

The top-level firmware targets accept the same switch. For example:

```bash
make firmware-build ARTEFACT_MODE=upstream FIRMWARE_TARGET=DEBUG
```

On O6 and O6N, `FIRMWARE_TARGET=DEBUG` enables firmware `DEBUG()` output, but the
serial routing depends on the artefact mode:

- `ARTEFACT_MODE=upstream FIRMWARE_TARGET=DEBUG` keeps the imported upstream
  behavior.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG` keeps firmware `DEBUG()` output on
  UART2 by default.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG UART3_ENABLE=true` exposes UART3 to
  ACPI and muxes the header pins for UART use without moving firmware
  `DEBUG()` output off UART2.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG DEBUG_ON_UART3=true` opts into the
  custom overlay path that routes firmware `DEBUG()` output to UART3 and
  implies `UART3_ENABLE=true`.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG DEBUG_PRINT_ERROR_LEVEL=0x8000004f`
  widens the default custom debug mask while keeping the serial route
  unchanged.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=RELEASE DEBUG_VERBOSE=true` re-enables
  `DEBUG()` logging on RELEASE builds with a narrow property mask. When
  `DEBUG_PRINT_ERROR_LEVEL` is left unset in that mode, the build enables all
  available `DEBUG_*` message levels by default.

That UART3 option consumes 40-pin header GPIO105 and GPIO106 while it is
enabled. `DEBUG_ON_UART3`, `UART3_ENABLE`, `DEBUG_VERBOSE`, and
`DEBUG_PRINT_ERROR_LEVEL` are custom-only overrides; `ARTEFACT_MODE=upstream`
keeps the imported source behavior unchanged. Run `make help-debug` for the
derived `DEBUG_PRINT_ERROR_LEVEL` bit values from `DebugLib.h`. See
`docs/debug.md` for the board-specific serial details.

To validate the custom Microsoft Secure Boot defaults on real hardware, use
[`docs/secure-boot-hardware-validation.md`](secure-boot-hardware-validation.md)
as the field checklist after the build succeeds.

Edit `DSC` in `src/Makefile` to reduce amount of variants that will be built.
You should also edit `debian/edk2-cix.install` to exclude unbuild variants,
otherwise `debuild` will complain that those files are missing.

## Monorepo layout

On `main-monorepo-edk2`, the imported `edk2`, `edk2-platforms`, and
`edk2-non-osi` trees are regular directories inside this repo. There are
no Git submodules to initialize or update.

When `ARTEFACT_MODE=custom`, the build also prepends any matching package
roots under `custom/overlay/` to `PACKAGES_PATH`, so source-level custom
changes can shadow selected imported files without editing them in place.

The monorepo-edk2 build resolves its displayed top-level source hash and
default timestamp from the merge-base with
`main-monorepo-upstream-edk2`, so curated overlay commits do not change
the firmware's reported source identity. In the default
`ARTEFACT_MODE=custom`, that source-model commit timestamp is used
instead of wall-clock time. Run `make -C src print-build-metadata` to
inspect the resolved values.

If you need to force a specific reproducible timestamp for a custom
build, export `SOURCE_DATE_EPOCH=<unix-seconds>` before running the
build. The same value is also passed into the O6 `pm_config`
generator unless `PM_CONFIG_SOURCE_DATE_EPOCH` is set explicitly, so
`csu_pm_config.bin` stops depending on wall-clock time.

For a byte-identical rebuild of a previously published O6 or O6N image, use
`ARTEFACT_MODE=upstream`. The vendor build embeds three independent timestamp
domains, so set them explicitly and point
`SIGNING_CERT_SOURCE_DIR=<path-to-cert-bundle>` at either:

- `BUILD_DATE=<iso8601>` for the displayed firmware build timestamp
- `SOURCE_DATE_EPOCH=<unix-seconds>` for compiler-provided `__DATE__` and
  `__TIME__` uses
- `PM_CONFIG_SOURCE_DATE_EPOCH=<unix-seconds>` for the selected board's
  PM-config blob

- a build tree `certs/` directory containing `trusted_key_no.crt`,
  `nt_fw_cert.crt`, and `nt_fw_key.crt`
- an extracted FIP cert bundle containing `trusted-key-cert.bin`,
  `nt-fw-cert.bin`, and `nt-fw-key-cert.bin`

When `ARTEFACT_MODE=upstream` is combined with
`SIGNING_CERT_SOURCE_DIR=<path>`, that cert bundle is treated as required
replay input and the build now fails immediately if the directory is missing
or does not provide all three cert blobs.

The build also supports two output modes:

- `ARTEFACT_MODE=custom` is the default on `main-monorepo-edk2` and
  strips embedded PE/COFF debug path records from release firmware
  images
- `ARTEFACT_MODE=upstream` keeps the historical output behavior for
  the upstream vendor path, replay, and byte-for-byte comparison work

## Replay published firmware

To recover the replay settings from a published O6 or O6N release artefact and
write helper files under a fresh temp directory, run:

```bash
python3 src/scripts/replay_o6_release.py <edk2-cix_*.deb>
```

The helper extracts:

- `BUILD_DATE`
- `SOURCE_DATE_EPOCH`
- `PM_CONFIG_SOURCE_DATE_EPOCH`
- `SOURCE_COMMIT_HASH` plus the recorded sub-component hashes from
  `BuildOptions` when they are available
- a reusable FIP cert bundle for `SIGNING_CERT_SOURCE_DIR`

When those explicit replay inputs are provided, the build no longer needs a
usable Git checkout just to resolve metadata. That keeps copied trees and
containerized replay wrappers quiet and deterministic instead of probing a
host-side worktree that may not exist inside the container.

Without those explicit replay inputs, both `ARTEFACT_MODE=custom` and ordinary
`ARTEFACT_MODE=upstream` builds resolve `SOURCE_COMMIT_HASH`,
`SOURCE_DATE_EPOCH`, `PM_CONFIG_SOURCE_DATE_EPOCH`, and `BUILD_DATE` from the
mapped upstream source commit in Git history. On the custom path, that keeps
the displayed build metadata tied to the upstream tag or commit being built
rather than the local overlay commit.

Those ordinary `ARTEFACT_MODE=upstream` builds still stay on the upstream
vendor path. The explicit replay inputs are what promote that path from an
ordinary upstream build to a byte-identical rebuild of a published vendor
image.

It also writes:

- `replay.env`
- `rebuild-o6.sh`
- `rebuild-o6-docker.sh`

For the common qualification or replay workflow, the top-level Makefile wraps
this as:

```bash
make deterministic-replay \
  REPLAY_INPUT=/path/to/edk2-cix_1.2.1_all.deb
```

That target defaults to:

- `FIRMWARE_BOARD=O6`
- `FIRMWARE_DISTRO=bookworm`
- `REPLAY_VERSION=$(dpkg-parsechangelog -S Version)`

and then:

- seeds or reuses `.buildbox/replay/<profile>/`
- rebuilds with `ARTEFACT_MODE=upstream`
- runs `validate-firmware-strict` against the matching checked-in profile
- when the input is the published `1.2.1` release plus the extracted release
  cert bundle, qualifies the Bookworm path against the published release data

You can also switch to `FIRMWARE_DISTRO=trixie` for a same-input Trixie
replay. In that mode the goal is matching `amd64` and `arm64` outputs
against the same cert bundle and injected timestamps, not comparison with a
published upstream release.

When you already prepared `.buildbox/replay/<profile>/` once, later reruns can
omit `REPLAY_INPUT=...` and will reuse the cached `replay.env` plus cert
bundle. When the replay input is only `cix_flash_all.bin`, also provide
`REPLAY_BUILD_OPTIONS=/path/to/BuildOptions` when available so the helper can
recover `BUILD_DATE`.

To keep the full transcript from a replay or local build, wrap the command
with `./scripts/capture_build_log.sh build-logs <command ...>`. The
convenience target `make buildbox-firmware-log` does this for the standard
selected-board build.

For firmware builds, the underlying EDK2 build now defaults to silent mode so
you see the higher-level `Building ...` progress without the full compiler
command flood. That applies both to direct `make -C src ...` invocations and
to the top-level `make` targets that recurse into `src`. Use `V=1` on the
`make` command line to show the raw EDK2 command lines.

To produce deployable firmware files without creating a Debian package, the
top-level Makefile extensions now provide:

```bash
make firmware-stage
make install
make zip
make targz
```

Those targets reuse the same deployable board-specific payload that the package
ships, but the non-`deb` exports keep it as a self-contained versioned tree per
product. By default the examples below show the `O6` files inside
`dist/firmware/orion-o6/<version>/`:

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

By default:

- `make firmware-stage` writes to `dist/firmware/orion-o6/<version>/`
- `make install` writes to `/boot/efi/edk2/radxa/orion-o6/<version>/`
- `make zip` writes `dist/edk2-cix-orion-o6-<version>.zip`
- `make targz` writes `dist/edk2-cix-orion-o6-<version>.tar.gz`

`make install` is intended for a live system with a writable EFI system
partition. Debian package builds do not use this target; they collect files
directly from the build tree with `dh_install`.

Both archive formats store the payload under `orion-o6/<version>/` inside the
archive, so the archive members match the same product/version layout used by
the staged payload.

`make buildbox-firmware-stage` uses the same export path, and the staged files
appear directly in the host checkout under the default
`dist/firmware/orion-o6/<version>/` path because the buildbox bind-mounts the
repo root. You do not need to copy files back out of the container separately.

To deploy that staged payload manually onto a target ESP or removable FAT
volume, copy `dist/firmware/orion-o6/` into `edk2/radxa/` on that filesystem.
That gives you:

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
`fs0:\edk2\radxa\orion-o6\<version>\startup.nsh` directly to launch that
board's flash flow.

For custom-path builds, `O6N` now omits `X86EmulatorDxe` from the firmware
image, and custom `O6` non-`deb` exports scrub `tools/LoadOpRom.efi` with
`GenFw --zero` before staging it for manual OpROM debugging.

Use these variables to change the defaults:

- `FIRMWARE_BOARD`
- `FIRMWARE_PRODUCT`
- `FIRMWARE_VERSION`
- `FIRMWARE_STAGE_ROOT`
- `FIRMWARE_INSTALL_ROOT`
- `FIRMWARE_ARCHIVE_ROOT`

If you only have a standalone `cix_flash_all.bin`, the helper can still
recover the compiler and PM-config timestamps plus the cert bundle. Supply a
matching `BuildOptions` file, or pass `--build-date <iso8601>`, to produce a
complete replay build:

```bash
python3 src/scripts/replay_o6_release.py \
  cix_flash_all.bin \
  --build-options BuildOptions
```

Use `--board O6N` when replaying the O6N release artefacts.

To start the replay build immediately in the current shell, add
`--run-build`. If you are not already in a working build environment, run the
generated `rebuild-o6-docker.sh` wrapper instead. That wrapper reuses the
persistent `edk2-cix-buildbox` container, mounts the checkout at
`/workspaces/edk2-cix`, and therefore preserves the same embedded build paths
as the upstream release. By default it also mounts the helper's temp directory
into the container automatically. If you need a different host/container temp
mapping, set `EDK2_CIX_HOST_TMPDIR` and `EDK2_CIX_CONTAINER_TMPDIR` before
running the wrapper.
Despite the retained filename, the local helper scripts prefer a working Docker
engine first when they can positively distinguish it from Podman. Otherwise
they fall back to `docker` first on macOS and `podman` first on Linux, then
try the other runtime if needed. The generated replay wrapper still pins the
buildbox back to the validated amd64 Bookworm environment explicitly.

## Reuse the build container

For repeat local builds, keep a prepared amd64 build container around
instead of paying the full dependency bootstrap cost every time:
For repeat local builds, keep a prepared build container around instead
of paying the full dependency bootstrap cost every time:

```bash
make buildbox-up
make buildbox-metadata
make buildbox-firmware-build
make buildbox-firmware-stage
make buildbox-firmware-log
make buildbox-deb
make buildbox-zip
make buildbox-targz
make buildbox-validate-firmware
make buildbox-validate-firmware-strict
make buildbox-capture-validation-profile
```

The `buildbox-*` targets keep their host-side scratch space under
`.buildbox/`, write staged/archive outputs under `dist/`, and copy Debian
package artefacts into `dist/deb/`. You can also drive them from another
working directory with `make -C /path/to/checkout buildbox-...`.

`make devcontainer_setup` is now idempotent: it checks for the required Debian
packages first and skips the `apt` work when the environment is already ready.
When it does need to provision packages, it uses noninteractive `apt-get`
settings and suppresses recommends/suggests to keep the first-time bootstrap
output much cleaner.

The firmware build also tracks the userspace and toolchain context used for
the EDK2 `BaseTools/Source/C` helpers. If you switch between container
environments or distro generations, the next build rebuilds those helper tools
automatically instead of reusing stale host-built binaries.

The preferred local distribution for `x86_64` builds remains Debian
`bookworm`.

In the original upstream tree, native `arm64` / `aarch64` builds needed the
`trixie` buildbox because the shipped closed-source helpers were not portable
enough for the Bookworm arm64 path. On `main-monorepo`, those helpers are now
reimplemented from source, so the same default `bookworm` buildbox works for
both `amd64` and native `arm64` exact replay.

Native arm64 packaging no longer depends on the old closed-source
`cert_uefi_create_rsa` helper: both that tool and `fiptool` are now built from
source in-tree, and the flash-image packaging step uses the source
`cix_package_tool` implementation too. The source replacements now live under
`src/tools/`.

Custom builds also support a curated CIX release selector:

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  CIX_RELEASE=1.2
```

`CIX_RELEASE=1.2` (also accepted as `v1.2`) is a custom-only mode. It uses the
public CIX BIOS V1.2 source snapshot for TF-A and OP-TEE, stages the later
public CIX community-release `bootloader1.img` payload that matches community
hardware logs, and source-builds `bootloader2.img` during packaging. The
materialised curated sources live under `src/cix-v1.2/`.

This is a curated mode rather than an exact replay of one public CIX
superproject commit, because the best public source and payload provenance is
split across the public `bios` and release repositories. For the variable-level
summary of what this selector does and how it combines with the other custom
switches, see [`build-variables.md`](build-variables.md).

If you reuse existing cert blobs, native arm64 reproduces the checked-in
Bookworm qualification profile byte-for-byte and can also match the checked-in
Trixie same-input reproducibility profile. Freshly generated certs are
compatible but not byte-identical, because they inherently carry signing-time
entropy.

So:

- use the default Bookworm buildbox on either `x86_64` or `arm64` / `aarch64`
  for the standard `main-monorepo` environment or the exact Bookworm
  replay baseline
- use `FIRMWARE_DISTRO=trixie` for the Trixie family in general
  buildbox use or for exact deterministic replay
- use `BUILDBOX_IMAGE=mcr.microsoft.com/devcontainers/base:...` to override
  `FIRMWARE_DISTRO` with a specific image
- use `make -C src host-fiptool` to prebuild the vendored TF-A
  `fiptool` before the first packaging run
- use `make -C src host-cert-uefi-create-rsa` to prebuild the
  source replacement for the non-trusted FIP cert helper
- the dependency bootstrap now includes `libssl-dev`, because the source-built
  `fiptool` needs the OpenSSL development headers

To compare a local build against a checked-in validation profile, run:

```bash
make validate-firmware ARTEFACT_MODE=upstream
```

That loads the selected validation profile from
[validation/expected-hashes.json](https://github.com/radxa-pkg/edk2-cix/blob/main-monorepo/validation/expected-hashes.json),
checks the key shipped artefacts plus a few structural markers from the EFI
utility binaries, and writes a JSON report under `build-validation/`.

For the published O6N release baseline, select the shared Bookworm profile and
the `O6N` board:

```bash
make validate-firmware \
  ARTEFACT_MODE=upstream \
  FIRMWARE_BOARD=O6N \
  FIRMWARE_VALIDATION_PROFILE=upstream-1.2.1-bookworm
```

The same file also carries the `upstream-1.2.1-trixie` reproducibility
profile, intended for matching amd64 or arm64 Trixie replays on
`main-monorepo` when they reuse the same cert bundle and replay timestamps:

The profile name refers to the Trixie distro/toolchain family used for the
build. The cert bundle reused by that profile currently still comes from the
published Bookworm release, because that is the only upstream cert source we
have today.

```bash
make validate-firmware \
  ARTEFACT_MODE=upstream \
  FIRMWARE_VALIDATION_PROFILE=upstream-1.2.1-trixie
```

Use the shared `upstream-<version>-<distro>` form for replay-profile names on
this branch.

For deeper offline regression checks against the currently checked-in Bookworm
replay baselines, use:

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
- the ACPI regression audit baselines emitted AML tables plus rerun `iasl`
  warning and remark families
- `make check-offline-audit-baselines` verifies the checked-in offline audit
  baseline JSON structure before those audits run
- `make refresh-offline-audit-baselines ARTEFACT_MODE=upstream` refreshes both
  offline audit baseline files together from the already-built selected board
  tree for the active `FIRMWARE_VALIDATION_PROFILE`
- the `audit-bundle` maintainer target combines the qualification checks plus
  the payload metadata self-test, and optionally also runs the metadata branch
  consistency checker if `MONOREPO_META_ROOT=/path/to/main-monorepo-meta` is
  set

The deterministic replay workflow also uploads the generated
`build-validation/*.json` files as CI artefacts and writes a short Markdown job
summary listing the report statuses.

For a maintainer-driven full qualification pass on GitHub Actions, the tree
also includes `.github/workflows/maintainer-audit-bundle.yaml`. That manual
workflow runs the `buildbox-audit-bundle` target for a selected board/profile,
checks metadata consistency against `main-monorepo-meta`, uploads the
generated `build-validation/*.json` reports, and writes the same short
Markdown summary to the job page.

To snapshot the current build into a fresh profile JSON for later review or to
seed a future validation baseline under a profile name you choose, run:

```bash
make capture-validation-profile \
  FIRMWARE_VALIDATION_PROFILE=<new-profile-name>
```

Ordinary `ARTEFACT_MODE=upstream` builds with freshly generated certs or
non-replay timestamps are not expected to match either stored profile. The
top-level `firmware-build`, `firmware-stage`, `zip`, and `targz` targets
therefore skip validation by default. Use `make validate-firmware`,
`make validate-firmware-strict`, or `make deterministic-replay` explicitly when
you want qualification checks, or set `FIRMWARE_VALIDATE_ON_BUILD=true` to
restore the advisory post-build validation step.

The vendor `cix_package_tool` still writes ANSI colour escapes even when it is
not attached to a terminal and even when `NO_COLOR`, `CLICOLOR=0`, and
`TERM=dumb` are set. The packaging rules therefore normalise that binary's
output locally, rather than post-processing the whole build transcript.

If you need to refresh the monorepo from the authoritative uplifted source-model,
use the automation and runbooks on the separate `main-monorepo-meta`
branch rather than running `git submodule` commands in this checkout.

## Host-side Python checks

The Python helpers in this branch now run cleanly on recent host Python
versions without the previous `SyntaxWarning` noise from invalid escape
sequences. To re-check them outside the devcontainer, run:

```bash
python3 -W default -m compileall -q -f \
  src/edk2/BaseTools/Source/Python \
  src/edk2/BaseTools/Scripts \
  scripts
python3 -W default -m py_compile \
  src/edk2/ArmPlatformPkg/Scripts/Ds5/build_report.py
```

This intentionally excludes dormant legacy Python 2 tooling in unrelated
vendor directories, which would require a broader port rather than a warning
cleanup.
