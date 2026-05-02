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
  ACT_WORKFLOW=.github/workflows/custom-payload-metadata.yaml \
  ACT_JOB=test-custom-payload-metadata
make gha-act-run \
  ACT_WORKFLOW=.github/workflows/custom-payload-metadata.yaml \
  ACT_JOB=test-custom-payload-metadata
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

- Debian `trixie` on `x86_64` or `arm64` / `aarch64` for the default
  `source/unofficial/current` buildbox path, including `ARTEFACT_MODE=upstream`
- Debian `bookworm` remains supported when explicitly requested, but buildbox
  preflight warns because `trixie` is the default for edk2-rebased builds

The untouched upstream repo contents still need Debian `trixie` for native
`arm64` / `aarch64` builds because they shipped closed-source helper binaries.
`source/unofficial/current` replaces those helpers with source implementations and
standardizes buildbox execution on Debian `trixie` by default.

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
```

The local container helpers prefer a working Docker engine first when they can
clearly reach one that is not a Podman compatibility socket. Otherwise they
fall back to the platform default order: `docker` first on macOS and `podman`
first on Linux. If the preferred runtime is installed but not usable, they
automatically try the other one before failing. Set
`EDK2_CIX_CONTAINER_RUNTIME=docker` or `EDK2_CIX_CONTAINER_RUNTIME=podman` to
force a specific runtime.
By default the buildbox follows the host architecture and uses the Trixie base
image for every firmware build mode, including `ARTEFACT_MODE=upstream`.
Override that with `FIRMWARE_DISTRO=bookworm`, `FIRMWARE_DISTRO=trixie`,
`EDK2_CIX_BUILDBOX_PLATFORM`, `EDK2_CIX_BUILDBOX_IMAGE`,
`BUILDBOX_PLATFORM=...`, or `BUILDBOX_IMAGE=...` when you need a specific
container environment.

To select the distro generation explicitly:

- use `FIRMWARE_DISTRO=bookworm` to force Bookworm for any buildbox run
- use `FIRMWARE_DISTRO=trixie` to force Trixie for any buildbox run
- use `BUILDBOX_IMAGE=mcr.microsoft.com/devcontainers/base:...` (or
  `EDK2_CIX_BUILDBOX_IMAGE=...`) to override the image directly;
  when unset, the effective default is
  `mcr.microsoft.com/devcontainers/base:trixie`
The firmware-oriented `buildbox-*` targets install the slimmer firmware
dependency profile inside the reusable container; `buildbox-deb` switches that
same container to the fuller packaging profile when needed.

For non-buildbox workflows, the base OS is whichever environment you are
already building in. Host-native builds therefore use the host distro, and the
checked-in devcontainer remains pinned by
[`/.devcontainer/devcontainer.json`](https://github.com/radxa-pkg/edk2-cix/blob/source/unofficial/edk2-stable202302/.devcontainer/devcontainer.json).

## Build inside a devcontainer

We still use devcontainers to keep one known-good amd64 environment around.

To build all supported EDK2 variants inside that environment, run:

```bash
make deb
```

`source/unofficial/current` only supports Linux build hosts now. The old vendor
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

On `source/unofficial/current`, the imported `edk2`, `edk2-platforms`, and
`edk2-non-osi` trees are regular directories inside this repo. There are
no Git submodules to initialize or update.

When `ARTEFACT_MODE=custom`, the build also prepends any matching package
roots under `custom/overlay/` to `PACKAGES_PATH`, so source-level custom
changes can shadow selected imported files without editing them in place.

Rendered builds resolve their displayed top-level source hash and default
timestamp from the Source-Base trailer recorded during materialisation,
so curated overlay commits do not change the firmware's reported source
identity. In the default
`ARTEFACT_MODE=custom`, that source-model commit timestamp is used
instead of wall-clock time. Run `make -C src print-build-metadata` to
inspect the resolved values.

If you need to force a specific reproducible timestamp for a custom
build, export `SOURCE_DATE_EPOCH=<unix-seconds>` before running the
build. The same value is also passed into the O6 `pm_config`
generator unless `PM_CONFIG_SOURCE_DATE_EPOCH` is set explicitly, so
`csu_pm_config.bin` stops depending on wall-clock time.

`source/unofficial/current` is rebased onto a newer upstream EDK2 implementation, so
byte-accurate replay of the published 202208-based vendor images is not a valid
proof target on this branch. The deterministic replay, exact hash-validation,
and offline replay-baseline audit targets are retained for future use but fail
early here; use `source/unofficial/edk2-stable202208` for byte-accurate vendor replay.

For a closest-to-upstream rebuild on the rebased implementation, use
`ARTEFACT_MODE=upstream`. The vendor build still embeds three independent
timestamp domains, so set them explicitly and point
`SIGNING_CERT_SOURCE_DIR=<path-to-cert-bundle>` at either if you need stable
inputs for diagnosis:

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

- `ARTEFACT_MODE=custom` is the default on `source/unofficial/current` and
  strips embedded PE/COFF debug path records from release firmware
  images
- `ARTEFACT_MODE=upstream` keeps the closest-to-upstream vendor path on the
  rebased EDK2 implementation, but does not claim byte-for-byte equivalence with
  the older published vendor releases

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

Those ordinary `ARTEFACT_MODE=upstream` builds still stay on the
closest-to-upstream path. On `source/unofficial/current`, the explicit replay inputs
remain useful for metadata-stable diagnostics, but they do not promote the
rebased branch into a byte-identical rebuild of a published vendor image.

It also writes:

- `replay.env`
- `rebuild-o6.sh`
- `rebuild-o6-docker.sh`

The helper remains useful for extracting release metadata and cert bundles for
diagnostic comparisons. The top-level deterministic replay wrapper is disabled
on `source/unofficial/current`:

```bash
make deterministic-replay
```

It fails immediately with an explanation that deterministic builds are not valid
when the EDK2 implementation has been rebased.

On non-rebased branches, a prepared `.buildbox/replay/<profile>/` cache lets
later deterministic replay reruns omit `REPLAY_INPUT=...` and reuse the cached
`replay.env` plus cert bundle. On `source/unofficial/current`, keep those cached inputs
only for diagnostic extraction; the Makefile replay target remains disabled.

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
`dist/firmware/edk2/radxa/orion-o6/<version>/`:

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

- `make firmware-stage` writes to `dist/firmware/edk2/radxa/orion-o6/<version>/`
- `make install` writes to `/boot/efi/edk2/radxa/orion-o6/<version>/`
- `make zip` writes `dist/edk2-cix-orion-o6-<version>.zip`
- `make targz` writes `dist/edk2-cix-orion-o6-<version>.tar.gz`
- `make build-all` aliases `make build-all-targz` and writes
  `dist/edk2-cix-orion-o6-<version>-all.tar.gz`
- `make build-all-zip` writes `dist/edk2-cix-orion-o6-<version>-all.zip`

`make install` is intended for a live system with a writable EFI system
partition. Debian package builds do not use this target; they collect files
directly from the build tree with `dh_install`.

For non-`deb` builds, firmware versioning now comes from the repo-root
`VERSION` file. `make deb` validates that the Debian changelog upstream
version matches it instead of defining the firmware version for every other
target.

Both archive formats store the payload under `edk2/radxa/orion-o6/<version>/`
inside the archive, so the archive members match the same product/version
layout used by the staged payload. `build-all` is buildbox-orchestrated and all
leaves, including the upstream diagnostic leaf, default to Trixie on
`source/unofficial/current`.
The traversal keeps every RELEASE variant ahead of every DEBUG variant, walks
the broader custom invalidators before the UART/debug leaves, and then toggles
the narrower curated `cix` path inside each compile bucket to reduce avoidable
rebuild churn. `build-all` archives the curated preset leaves for each
container instead of every legal flag combination.
Single-build custom exports reuse the same subtree naming scheme, including
curated `custom/cix/...` leaves when `CIX_RELEASE=v1.2` is selected, even when
you request a combination that is not part of the preset variant set.

`make buildbox-firmware-stage` uses the same export path, and the staged files
appear directly in the host checkout under the default
`dist/firmware/edk2/radxa/orion-o6/<version>/` path because the buildbox
bind-mounts the repo root. You do not need to copy files back out of the
container separately.

To deploy that staged payload manually onto a target ESP or removable FAT
volume, copy `dist/firmware/edk2/` into that filesystem root. That gives you:

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
try the other runtime if needed. The generated replay wrapper is retained for
non-rebased replay investigations, but the supported `source/unofficial/current`
buildbox paths use Trixie by default.

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

The preferred buildbox distribution for `source/unofficial/current` is Debian
`trixie`.

In the original upstream tree, native `arm64` / `aarch64` builds needed the
`trixie` buildbox because the shipped closed-source helpers were not portable
enough for the Bookworm arm64 path. On `source/unofficial/current`, those helpers are
now reimplemented from source, but Trixie remains the default buildbox base for
both upstream and custom builds.

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
  CIX_RELEASE=v1.2
```

`CIX_RELEASE=v1.2` is a custom-only mode. It uses the public CIX BIOS V1.2
source snapshot for TF-A and OP-TEE, stages the later
public CIX community-release `bootloader1.img` payload that matches community
hardware logs, and source-builds `bootloader2.img` during packaging. The
materialised curated sources live under `src/cix-v1.2/`.

This is a curated mode rather than an exact replay of one public CIX
superproject commit, because the best public source and payload provenance is
split across the public `bios` and release repositories. For the variable-level
summary of what this selector does and how it combines with the other custom
switches, see [`build-variables.md`](build-variables.md).

If you reuse existing cert blobs, the build can keep signing inputs stable for
diagnostic comparisons. It is still not a byte-accurate replay of the published
vendor firmware because the EDK2 implementation itself has been rebased.

So:

- use the default Trixie buildbox for both upstream-oriented and custom buildbox
  runs
- use `FIRMWARE_DISTRO=bookworm` only when deliberately checking older distro
  compatibility; the preflight will warn on `source/unofficial/current`
- use `BUILDBOX_IMAGE=mcr.microsoft.com/devcontainers/base:...` to override
  `FIRMWARE_DISTRO` with a specific image
- use `make -C src host-fiptool` to prebuild the vendored TF-A
  `fiptool` before the first packaging run
- use `make -C src host-cert-uefi-create-rsa` to prebuild the
  source replacement for the non-trusted FIP cert helper
- the dependency bootstrap now includes `libssl-dev`, because the source-built
  `fiptool` needs the OpenSSL development headers

The checked-in byte-accurate validation profiles and offline replay baselines
belong to the non-rebased `source/unofficial/edk2-stable202208` history. On `source/unofficial/current`, the
following targets are intentionally hidden from help and fail early if invoked:

- `deterministic-replay`
- `validate-firmware`
- `validate-firmware-strict`
- `capture-validation-profile`
- `check-offline-audit-baselines`
- `audit-final-image-manifest`
- `audit-acpi-regression`
- `refresh-offline-audit-baselines`
- the corresponding `buildbox-*` validation and audit wrappers

The tree still contains the maintainer replay/audit GitHub Actions workflow
used by non-rebased histories, but those byte-accurate replay checks are not a
valid qualification target for `source/unofficial/current` and will fail through the
same guard if invoked here.

Capturing new byte-accurate validation profiles is disabled on this branch for
the same rebased-EDK2 reason.

Ordinary `ARTEFACT_MODE=upstream` builds remain supported for closest-to-upstream
diagnostics. The top-level `firmware-build`, `firmware-stage`, `zip`, and
`targz` targets skip byte-accurate validation, and
`FIRMWARE_VALIDATE_ON_BUILD=true` fails early on this branch for the same
rebased-EDK2 reason.

The vendor `cix_package_tool` still writes ANSI colour escapes even when it is
not attached to a terminal and even when `NO_COLOR`, `CLICOLOR=0`, and
`TERM=dumb` are set. The packaging rules therefore normalise that binary's
output locally, rather than post-processing the whole build transcript.

If you need to refresh the monorepo from the authoritative uplifted source-model,
use the automation and runbooks on the separate `build`
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
