# EDK2-CIX Firmware Build

This branch contains the Makefile, manifests, and scripts used to select source
versions and build firmware for the Radxa Orion O6 and O6N boards. The source
model combines:

- upstream bases such as EDK2, TF-A, and OP-TEE
- Radxa firmware source layers
- recorded CIX v1.2 BL1, TF-A and OP-TEE development inputs; source-built
  trusted components cannot be packaged for locked boards without vendor signing authority
- unofficial project changes
- generated materialised firmware worktrees that are ready to build

Users who want to build firmware, rather than change the source model, should
find that the `make` targets provide the flexibility they need. In this
document, a Git "ref" means a name that identifies a commit, such as a branch,
tag, or commit ID. Start with:

```bash
make help
make help-vars
make help-source-targets
```

## Licensing

Project-authored build-branch material is distributed under the GNU General
Public License, version 3; see `LICENSE`. Each retained source ref also carries
its own upstream or vendor licensing and copyright information. Those
component-specific terms continue to govern that component: the root licence
does not replace or reinterpret them.

## What does a bare `make` build?

A targetless `make` uses the `upstream` profile. It rebuilds the latest
published Radxa release from its exact EDK2 202208 source target and recorded
replay data, then compares the rebuilt payload byte-for-byte with the published
package. The current default is Radxa `1.3.1` for O6:

```bash
make
```

Select O6N explicitly:

```bash
make FIRMWARE_BOARD=O6N
```

The command reports the selected release and source target before doing any
work. On success, it reports that the strict byte comparison succeeded and
mirrors the useful raw outputs beneath `dist/build/upstream/`. It does not use
the CIX v1.2 early-boot replacement, a newer EDK2 base, or any project firmware
fix.

To build from the latest maintained source stack instead, select the `latest`
profile:

```bash
make PROFILE=latest
make PROFILE=latest FIRMWARE_BOARD=O6N
```

This currently selects EDK2 `202608` and Radxa `1.3.1`, retaining the
matching vendor-signed BL1, BL31 and OP-TEE payloads. It uses the project's custom-capable build path, but
keeps `ENABLE_FIRMWARE_FIXES=false`; the result is an uplifted current-source
build and is not expected to match the published 202208-based Radxa image
byte-for-byte. Enable the opinionated fixes separately:

```bash
make PROFILE=latest ENABLE_FIRMWARE_FIXES=true
```

User-facing build and packaging targets use the buildbox by default, so the
host does not need an AArch64 firmware toolchain installed.

## How do I select a source build directly?

The lower-level source-build targets default to the latest maintained source
target. For a single firmware build, choose the board and build target:

```bash
make build FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE
make build FIRMWARE_BOARD=O6N FIRMWARE_TARGET=RELEASE
```

Common variables are:

- `FIRMWARE_BOARD=O6|O6N` selects the board. The default is `O6`.
  Product paths and archive names follow this selection: `orion-o6` or
  `orion-o6n`. No separate product argument is needed.
- `FIRMWARE_TARGET=RELEASE|DEBUG` selects a release or debug firmware image.
  The default is `RELEASE`.
- `RELEASE=<source-target>` selects a configured source target. Leave this
  unset on a lower-level build target to use the latest derived source target.
- `ARTEFACT_MODE=custom|upstream` selects the build and artefact mode inside
  the chosen source tree. The default is `custom`.
- `V=1` enables verbose script and delegated build output. The default, `V=0`,
  keeps output concise.

`ARTEFACT_MODE=custom` means that the selected source tree permits project
source replacements and optional feature gates; it does not imply that
`ENABLE_FIRMWARE_FIXES` is enabled. `ARTEFACT_MODE=upstream` follows the
vendor-style path inside the selected tree. It does not select the historical
source or replay data by itself, so use the `upstream` profile or
`make deterministic-replay` when byte identity is the goal.

To stage the deployable payload under `dist/firmware/`, use:

```bash
make buildbox-firmware-stage FIRMWARE_BOARD=O6N FIRMWARE_TARGET=RELEASE
```

To create a single-board archive for the selected source target, use:

```bash
make zip FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE
make targz FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE
```

`make install` builds and stages one selected payload, then checks the selected
install root before copying anything. By default it installs under `/boot/efi`;
set `INSTALL_ROOT=/boot` or another path if your system uses a different mount
point. Existing firmware payload files under `INSTALL_ROOT` are never replaced
unless you rerun with `FORCE=1`.

```bash
make install FIRMWARE_BOARD=O6 INSTALL_ROOT=/boot/efi
make install FIRMWARE_BOARD=O6 INSTALL_ROOT=/boot/efi FORCE=1
```

`make build-all` is for producing a distributable bundle containing all
supported firmware build variants for the selected board and source target.
Here, a firmware build variant means one output selected by the rendered
firmware tree's own build matrix, not a different EDK2/CIX/Radxa source
combination. It deliberately ignores most single-image build variables because
it chooses that build-output set itself.

```bash
make build-all FIRMWARE_BOARD=O6
```

To verify that the replay-capable vendor source has not drifted away from a
published Radxa release, run the deterministic replay wrapper from this branch:

```bash
make deterministic-replay FIRMWARE_BOARD=O6
make deterministic-replay FIRMWARE_BOARD=O6N
```

By default, this renders the exact `edk2-202208/radxa-1.3.1` upstream source
target, downloads the matching `1.3.1` release package from
`radxa-pkg/edk2-cix`, and delegates to the rendered firmware tree's strict
byte-identical replay target. To replay a package you already have, pass
`REPLAY_INPUT=/path/to/edk2-cix_1.3.1_all.deb`; for a raw
`cix_flash_all.bin`, also pass `REPLAY_BUILD_OPTIONS=/path/to/BuildOptions`
when available.

The replay-capable EDK2 202208 source records timestamp, certificate, package,
and validation-profile data for Radxa `1.2.1`, `1.2.2`, `1.2.3`, `1.2.4`,
`1.3.0`, and `1.3.1`, for both O6 and O6N. Reuse is content-addressed: the
trusted-key certificate is shared across those releases because its SHA-256 is
identical, while release- and board-specific certificate data remains distinct.

## How do I choose EDK2 and Radxa source versions?

A source target selects EDK2, Radxa, and unofficial project sources to build.
Matching vendor-signed early-boot payloads are retained. Use
`make help-source-targets` to list the configured combinations.

```bash
make help-source-targets
```

The minimum maintained scope is:

| Build mode | EDK2 | Radxa |
| --- | --- | --- |
| Stock upstream / exact replay | 202208 | 1.2.1, 1.2.2, 1.2.3, 1.2.4, 1.3.0, 1.3.1 |
| Custom `/unofficial` | 202208 and latest maintained (currently 202608) | 1.2.4 and current maintained (currently 1.3.1) |

Each stock target uses the matching `source/vendor/radxa/<version>/edk2-stable202208`
ref. Its 202208 `/unofficial` counterpart adds our source changes to that exact
Radxa release; no EDK2 uplift is involved. Optional firmware fixes still require
`ENABLE_FIRMWARE_FIXES=true`. Custom output is not a byte-identical stock replay,
even with optional fixes disabled.

The custom qualification matrix follows deliberate promotion of the maintained
EDK2/Radxa stack, keeping 202208 and Radxa 1.2.4 as fixed baselines. Merely
discovering a newer upstream tag does not change it. Older stock releases remain
in replay coverage when the current release advances. Other historical source
targets are retained, with their qualification limits shown separately in help.

For example, rebuild and byte-compare stock Radxa 1.2.4 with:

```bash
make deterministic-replay REPLAY_VERSION=1.2.4 FIRMWARE_BOARD=O6
```

Use the `RELEASE` variable to select one of those combinations. The documented
form is the prefixless source-target name shown by `make help-source-targets`:

```bash
make build \
  RELEASE=edk2-202608/radxa-1.3.1/unofficial \
  FIRMWARE_BOARD=O6N \
  FIRMWARE_TARGET=RELEASE
```

Build targets render or reuse a cached detached worktree. They do not normally
create or advance a named source branch, so selecting a source target requires
no repository-maintenance step.

Downloads and compiler caches are reused beneath `.cache/edk2-cix/firmware/`.
Identical source renders reuse the same detached worktree. Custom builds clean
their generated build directory by default; for a deliberate incremental
debugging run, add `EDK2_CIX_INCREMENTAL_CUSTOM_WORKSPACE=1`.

To inspect or modify the prepared source tree, resolve its path once, then run
its build target directly. This also avoids the renderer rejecting intentional
edits to a cached tree:

```bash
firmware_tree="$(python3 scripts/render_release_branch.py \
  --release edk2-202605/radxa-1.3.1/unofficial --print-worktree)"
firmware_cache="$PWD/.cache/edk2-cix/firmware/buildbox"
make -C "$firmware_tree" buildbox-firmware-build \
  ARTEFACT_MODE=custom FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE \
  FIRMWARE_DISTRO=trixie ENABLE_FIRMWARE_FIXES=false ENABLE_CORE_ORDER=cix \
  ENABLE_EXPERIMENTAL_UEFI_SETTINGS=false DEBUG_VERBOSE=false CIX_RELEASE= \
  EDK2_CIX_INCREMENTAL_CUSTOM_WORKSPACE=1 \
  BUILDBOX_HOST_TMPDIR="$firmware_cache" BUILDBOX_CONTAINER_TMPDIR=/hosttmp \
  CCACHE_DIR=/hosttmp/ccache CCACHE_WRAPPER_ROOT=/hosttmp/ccache-toolchain \
  CIX_RELEASE_CACHE_ROOT=/hosttmp/cix-release
```

That direct debugging build leaves firmware under
`$firmware_tree/src/Build/O6/RELEASE_GCC*/`. Normal top-level `make build`
also copies outputs beneath `dist/build/<release>/<profile>/<board>/`.
The complete flash image is `cix_flash_all.bin`. Neither command flashes a device.

## Repository maintenance

For persistent materialised branches, source-model internals, firmware-source
development, upstream integration and uplift, coordinated ref publication,
CI, and maintainer validation, see [`MAINTENANCE.md`](MAINTENANCE.md).

### Trusted firmware qualification

The build verifies BL1 provenance and the trusted/UEFI FIP certificate chains,
payload digests, delegation and reference counters before publishing images.
Every nonblank `CIX_RELEASE` value is rejected immediately by Make: available
keys cannot sign modified trusted-world firmware under the vendor chain. Leave
`CIX_RELEASE=` to retain the vendor BL31/OP-TEE payloads. The `latest` profile
and distributable bundles use this qualified path. Curated component source
compilation and rejection of its incompatible signature are tested separately.
See [firmware chain validation](docs/src/firmware-chain-validation.md) for the
scope of these checks and their limits.
