# Build

You can build this repo either directly on a supported Debian host or inside a
containerized environment.

If you want a quick overview of the common top-level workflows first, run:

```bash
make help
```

For lower-level firmware targets, use:

```bash
make -C src help
```

## Supported host environments

- Debian `bookworm` on `x86_64` for the preferred exact-upstream replay path
- Debian `trixie` on `arm64` / `aarch64` for native local builds
- Debian `bookworm` on `arm64` / `aarch64` for exact replay when you reuse the
  extracted cert bundle

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

If you prefer to reuse a warmed build container instead of installing the
dependencies onto the host, use:

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
By default the buildbox follows the host architecture: `linux/amd64` with the
Bookworm base image on `x86_64`, and `linux/arm64` with the Trixie base image
on `arm64` / `aarch64`. Override that with `EDK2_CIX_BUILDBOX_PLATFORM`,
`EDK2_CIX_BUILDBOX_IMAGE`, `BUILDBOX_PLATFORM=...`, or
`BUILDBOX_IMAGE=...` when you need a specific container environment.
The firmware-oriented `buildbox-*` targets install the slimmer firmware
dependency profile inside the reusable container; `buildbox-deb` switches that
same container to the fuller packaging profile when needed.

## Build inside a devcontainer

We still use devcontainers to keep one known-good amd64 environment around.

To build all supported EDK2 variants inside that environment, run:

```bash
make deb
```

`main-monorepo` only supports Linux build hosts now. The old vendor
`WinBuildTool` tree and its Windows-only helper makefiles were removed from
this branch, so the supported local host environments are:

- Linux `x86_64`
- Linux `aarch64` / `arm64`

Before a longer build, run `make -C src preflight` to fail early if the
expected package-tool binaries, source directories, or cross-compiler are
missing.

Set `BUILD_TARGET` to `DEBUG` in `src/Makefile` to build for debug artifacts.

Edit `DSC` in `src/Makefile` to reduce amount of variants that will be built.
You should also edit `debian/edk2-cix.install` to exclude unbuild variants,
otherwise `debuild` will complain that those files are missing.

## Monorepo layout

On `main-monorepo`, the imported `edk2`, `edk2-platforms`, and
`edk2-non-osi` trees are regular directories inside this repo. There are
no Git submodules to initialize or update.

The monorepo build resolves its displayed top-level source hash and default
timestamp from the nearest mapped upstream `main` commit in history, so
monorepo-only build-system commits do not change the firmware's reported
source identity. In the default `ARTEFACT_MODE=custom`, that upstream commit
timestamp is used instead of wall-clock time. Run `make -C src
print-build-metadata` to inspect the resolved values.

If you need to force a specific reproducible timestamp for a custom build,
export `SOURCE_DATE_EPOCH=<unix-seconds>` before running the build. The same
value is also passed into the O6 `pm_config` generator unless
`PM_CONFIG_SOURCE_DATE_EPOCH` is set explicitly, so `csu_pm_config.bin`
stops depending on wall-clock time.

For exact replay of a previously published O6 image, we found that the vendor
build embeds three independent timestamp domains. Set them explicitly and
point `SIGNING_CERT_SOURCE_DIR=<path-to-cert-bundle>` at either:

- `BUILD_DATE=<iso8601>` for the displayed firmware build timestamp
- `SOURCE_DATE_EPOCH=<unix-seconds>` for compiler-provided `__DATE__` and
  `__TIME__` uses
- `PM_CONFIG_SOURCE_DATE_EPOCH=<unix-seconds>` for the O6 PM-config blob

- a build tree `certs/` directory containing `trusted_key_no.crt`,
  `nt_fw_cert.crt`, and `nt_fw_key.crt`
- an extracted FIP cert bundle containing `trusted-key-cert.bin`,
  `nt-fw-cert.bin`, and `nt-fw-key-cert.bin`

When `ARTEFACT_MODE=upstream` is combined with
`SIGNING_CERT_SOURCE_DIR=<path>`, that cert bundle is treated as required
replay input and the build now fails immediately if the directory is missing
or does not provide all three cert blobs.

The build also supports two output modes:

- `ARTEFACT_MODE=custom` is the default on `main-monorepo` and strips
  embedded PE/COFF debug path records from release firmware images
- `ARTEFACT_MODE=upstream` keeps the historical output behavior for
  replay and byte-for-byte comparison work

## Replay published O6 firmware

To recover the replay settings from a published O6 release artefact and write
helper files under a fresh temp directory, run:

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

It also writes:

- `replay.env`
- `rebuild-o6.sh`
- `rebuild-o6-docker.sh`

If you want to keep the full transcript from a replay or local build, wrap the
command with `./scripts/capture_build_log.sh build-logs <command ...>`. The
convenience target `make buildbox-o6-log` does this for the standard local O6
build.

For firmware builds, the underlying EDK2 build now defaults to silent mode so
you see the higher-level `Building ...` progress without the full compiler
command flood. That applies both to direct `make -C src ...` invocations and
to the top-level `make` targets that recurse into `src`. Use `V=1` on the
`make` command line if you want the raw EDK2 command lines.

If you want deployable firmware files without creating a Debian package, the
top-level Makefile extensions now provide:

```bash
make firmware-stage
make install
make zip
make targz
```

Those targets reuse the same deployable `O6` payload that the package ships:

- top-level `startup.nsh`
- `orion-o6/BuildOptions`
- `orion-o6/cix_flash_all.bin`
- `orion-o6/cix_flash_ota.bin`
- `orion-o6/BurnImage.efi`
- `orion-o6/FlashUpdate.efi`
- `orion-o6/EnrollFromDefaultKeysApp.efi`
- `orion-o6/VariableInfo.efi`
- `orion-o6/Shell.efi`
- `orion-o6/startup.nsh`

By default:

- `make firmware-stage` writes to `dist/firmware/orion-o6/<version>/`
- `make install` writes to `/boot/efi/firmware/radxa/<version>/`
- `make zip` writes `dist/edk2-cix-orion-o6-<version>.zip`
- `make targz` writes `dist/edk2-cix-orion-o6-<version>.tar.gz`

`make install` is intended for a live system with a writable EFI system
partition. Debian package builds do not use this target; they collect files
directly from the build tree with `dh_install`.

Both archive formats store the payload under `orion-o6/<version>/` inside the
archive, so the archive members match the same product/version layout used by
the staged payload.

`make buildbox-firmware-stage` uses the same export path, and the staged files
appear directly in the host checkout under `dist/firmware/orion-o6/<version>/`
because the buildbox bind-mounts the repo root. You do not need to copy files
back out of the container separately.

To deploy that staged payload manually onto a target ESP or removable FAT
volume, copy the contents of `dist/firmware/orion-o6/<version>/` into
`edk2/radxa/` on that filesystem. That layout matches the payload shipped by
the Debian package:

- `edk2/radxa/startup.nsh`
- `edk2/radxa/orion-o6/BuildOptions`
- `edk2/radxa/orion-o6/cix_flash_all.bin`
- `edk2/radxa/orion-o6/cix_flash_ota.bin`
- `edk2/radxa/orion-o6/BurnImage.efi`
- `edk2/radxa/orion-o6/FlashUpdate.efi`
- `edk2/radxa/orion-o6/EnrollFromDefaultKeysApp.efi`
- `edk2/radxa/orion-o6/VariableInfo.efi`
- `edk2/radxa/orion-o6/Shell.efi`
- `edk2/radxa/orion-o6/startup.nsh`

From the UEFI Shell, run `startup.nsh` to get the product picker, or run
`orion-o6/startup.nsh` directly to launch that board's flash flow.

Use these variables to change the defaults:

- `FIRMWARE_BOARD`
- `FIRMWARE_PRODUCT`
- `FIRMWARE_VERSION`
- `FIRMWARE_STAGE_ROOT`
- `FIRMWARE_INSTALL_ROOT`
- `FIRMWARE_ARCHIVE_ROOT`

If you only have a standalone `cix_flash_all.bin`, the helper can still
recover the compiler and PM-config timestamps plus the cert bundle. Supply a
matching `BuildOptions` file, or pass `--build-date <iso8601>`, if you want a
complete replay build:

```bash
python3 src/scripts/replay_o6_release.py \
  cix_flash_all.bin \
  --build-options BuildOptions
```

To start the replay build immediately in the current shell, add
`--run-build`. If you are not already in a working build environment, run the
generated `rebuild-o6-docker.sh` wrapper instead. That wrapper reuses the
persistent `edk2-cix-buildbox` container, mounts the checkout at
`/workspaces/edk2-cix`, and therefore preserves the same embedded build paths
as the upstream release. By default it also mounts the helper's temp directory
into the container automatically. If you need a different host/container temp
mapping, set `EDK2_CIX_HOST_TMPDIR` and `EDK2_CIX_CONTAINER_TMPDIR` before
running the wrapper.
Despite the retained filename, the local helper scripts prefer `podman` on
Linux and `docker` on macOS, then fall back to the other runtime if needed.
The generated replay wrapper also pins the buildbox back to the validated
amd64 Bookworm environment explicitly.

## Reuse the build container

For repeat local builds, keep a prepared build container around instead
of paying the full dependency bootstrap cost every time:

```bash
make buildbox-up
make buildbox-metadata
make buildbox-firmware-build
make buildbox-firmware-stage
make buildbox-o6
make buildbox-o6-log
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

Native arm64 packaging is now viable on newer userspaces, but the distro
generation still matters. The self-generated FIP-cert path no longer depends on
the old closed-source `cert_uefi_create_rsa` helper: both that tool and
`fiptool` are now built from source in-tree, and the flash-image packaging step
uses the source `cix_package_tool` implementation too.

If you reuse existing cert blobs, native arm64 Bookworm reproduces the
checked-in exact-replay `O6` baseline byte-for-byte. Freshly generated certs
are compatible but not byte-identical, because they inherently carry
signing-time entropy. A native arm64 Trixie userspace is still the broader
default when you want the newest distro toolchain for general development.

So:

- use the amd64 Bookworm buildbox, or an arm64 Bookworm buildbox with reused
  extracted cert blobs, when you need exact upstream replay
- use the default arm64 Trixie buildbox, or direct Trixie host builds, for
  broader native `arm64` / `aarch64` work
- use `make -C src host-fiptool` if you want to prebuild the vendored TF-A
  `fiptool` before the first packaging run
- use `make -C src host-cert-uefi-create-rsa` if you want to prebuild the
  source replacement for the non-trusted FIP cert helper
- the dependency bootstrap now includes `libssl-dev`, because the source-built
  `fiptool` needs the OpenSSL development headers

`bookworm` and `bookworm-backports` do not currently expose `gcc-13` or
`gcc-14` for the Debian `aarch64-linux-gnu` cross compiler in the default
repositories.

To compare a local build against the checked-in exact-replay baseline, run:

```bash
make validate-firmware ARTEFACT_MODE=upstream
```

That loads the `upstream-o6-1.2.1-bookworm` profile from
[validation/o6/expected-hashes.json](/Users/Stuart/src/edk2-cix/validation/o6/expected-hashes.json),
checks the key shipped artefacts plus a few structural markers from the EFI
utility binaries, and writes a JSON report under `build-validation/`.

For the current Trixie-era local build family there is also a structural-only
profile in the same file:

```bash
make validate-firmware FIRMWARE_VALIDATION_PROFILE=modern-o6-trixie-structural
```

That profile keeps `BuildOptions` exact, but checks the EFI utility binaries by
size and PE section layout rather than by whole-file hash.

To snapshot the current build into a fresh profile JSON for later review or to
seed a future validation baseline, run:

```bash
make capture-validation-profile \
  FIRMWARE_VALIDATION_PROFILE=modern-o6-trixie-structural
```

The top-level `firmware-build`, `firmware-stage`, `zip`, and `targz` targets
now run that validation automatically when `ARTEFACT_MODE=upstream`, so a
local build that drifts away from the stored Bookworm replay baseline emits a
very obvious warning even if the build itself completed successfully.

The vendor `cix_package_tool` still writes ANSI colour escapes even when it is
not attached to a terminal and even when `NO_COLOR`, `CLICOLOR=0`, and
`TERM=dumb` are set. The packaging rules therefore normalise that binary's
output locally, rather than post-processing the whole build transcript.

If you need to refresh the monorepo from the untouched upstream mirror,
use the automation and runbooks on the separate `main-monorepo-meta`
branch rather than running `git submodule` commands in this checkout.

## Host-side Python checks

The maintained Python helpers now run cleanly on recent host Python versions
without the previous `SyntaxWarning` noise from invalid escape sequences. To
re-check that set outside the devcontainer, run:

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
