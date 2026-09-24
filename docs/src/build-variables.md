# Build Variables

This page explains what each user-facing build variable changes on your O6 or
O6N system. The short `make help-vars` output is a quick reminder; this
document covers the side effects, compatibility rules, and typical use of each
variable.

## The Main Build Choices

Start with these variables.

### `PROFILE=upstream|latest`

Select the high-level behavior of a targetless `make`.

- `PROFILE=upstream`
   - the default
   - rebuild the latest published Radxa release from its exact EDK2 202208
    source and recorded replay inputs
   - compare the rebuilt payload byte-for-byte with the published package
   - reject active custom firmware options
- `PROFILE=latest`
   - build the latest maintained EDK2/Radxa source combination
   - preserve the matching signed vendor early-boot payloads
   - leave `ENABLE_FIRMWARE_FIXES=false` unless explicitly enabled
   - produce a current-source build, not a byte-identical Radxa reconstruction

Default: `upstream`

### `ARTEFACT_MODE=custom|upstream`

This is the lower-level build-path switch used by explicit build targets.

- `ARTEFACT_MODE=upstream`
   - keep the upstream vendor build path
   - this is the right choice for qualification, comparison against a
    published vendor release, or byte-identical replay with the published
    replay inputs
   - custom-only feature switches are rejected in this mode
- `ARTEFACT_MODE=custom`
   - keep the same overall build flow, but allow the local overlays, source
    replacements, and opt-in firmware changes
   - this is the right choice for local overlays and opt-in firmware changes
    instead of an upstream-identical vendor image

Default for explicit source-build targets: `custom`

`ARTEFACT_MODE=upstream` is the mode that follows the upstream vendor build
path. When you also provide the extracted certs, timestamps, and other replay
inputs described in [`build.md`](build.md), this repo can use that mode to
rebuild the published vendor images byte-for-byte.

### `FIRMWARE_BOARD=O6|O6N`

Select the board image you want to build.

- `O6`
   - Radxa Orion O6
- `O6N`
   - Radxa Orion O6N

This affects:

- the generated firmware image
- board-specific ACPI and setup content
- replay and validation profiles
- which custom-only settings are accepted

Default: `O6`

### `FIRMWARE_TARGET=RELEASE|DEBUG`

Select the underlying EDK2 build target.

- `RELEASE`
   - the normal end-user firmware image
   - smallest runtime debug footprint
   - can still emit firmware logs if you opt into `DEBUG_VERBOSE=true`
- `DEBUG`
   - intended for bring-up and firmware debugging
   - enables the broader EDK2 debug/assert behaviour expected from a DEBUG
     build
   - does not imply that the result fits the vendor flash layout

Default: `RELEASE`

Custom builds start with the selected RELEASE FD capacity and increase it only
when the compressed FV requires more space. This also applies to full DEBUG
builds: the source FDF's 4 MiB DEBUG allocation is not treated as a minimum.
The generated FD must fit its reserved RAM, and the signed BL33 FIP must fit
the selected flash allocation. The unchanged vendor 1.3.1 FDF uses
`0x1f0000` for both targets; source-release layouts differ.

`FVMAIN` already uses LZMA compression inside `FVMAIN_COMPACT`, including its
debug strings. There is no separate string-dictionary compression switch.
For the audited Radxa 1.3.1 loader, `DEBUG_ALLOW_LARGE_IMAGE=1` permits an
actual signed FIP larger than `0x1f9000` to occupy up to `0x3fa000` bytes
starting at `0x406000`. It does not move earlier regions or waive signing,
physical flash or RAM limits. Enlarged layouts emit a full image only.
See [adaptive BL33 sizing](debug-layout.md) for the checks and qualification
limits; successful packaging still requires testing on hardware.

The original Radxa 1.3.1 AArch64 packager rejects oversized full-flash payloads
but accepts oversized OTA payloads, even one byte over the reserved UEFI slot.
Its original OTA Make target also reports success. The source replacement
checks both formats, and the independent final-image validator checks the
vendor slot bounds again. Successful certificate validation alone is not a
size/layout check.

### `FIRMWARE_DISTRO=bookworm|trixie`

Select the default distro family used by the buildbox helpers and deterministic
replay wrappers.

- `bookworm`
   - the default for `ARTEFACT_MODE=upstream`
   - also the default for deterministic replay when you do not override it
- `trixie`
   - the default for `ARTEFACT_MODE=custom`
   - the newer Debian/toolchain family for local feature work

This does not change the firmware feature set directly. It changes the build
environment used by the wrapper targets.

Default: `bookworm` for upstream and deterministic replay; `trixie` for custom

## Replay Variables

These variables are used by the build-branch `make deterministic-replay`
wrapper. The wrapper renders the replay-capable EDK2 202208 source target and
then delegates to the rendered firmware tree's strict replay target.

### `REPLAY_INPUT=<path>`

Select the vendor release input to replay.

Accepted inputs are:

- a published `edk2-cix_*.deb`
- an extracted release directory containing `cix_flash_all.bin`
- a raw `cix_flash_all.bin`

When unset, `make deterministic-replay` downloads the selected release package
from `REPLAY_UPSTREAM_REPOSITORY`, unless `REPLAY_DOWNLOAD=0` is set.

Default: unset

### `REPLAY_SOURCE_TARGET=<source-target>`

Select the source target to render before replay. Leave this at the default for
byte-identical vendor release replay.

Default: `edk2-202208/radxa-<REPLAY_VERSION>`

### `REPLAY_UPSTREAM_REPOSITORY=<owner/name>`

Select the GitHub repository used to resolve and download the release package
when `REPLAY_INPUT` is unset.

Default: `radxa-pkg/edk2-cix`

### `REPLAY_DOWNLOAD=0|1`

Control automatic release-package downloads when `REPLAY_INPUT` is unset.

- `1`
   - resolve and download the selected release package before replay
- `0`
   - do not download; delegate with no replay input so the rendered firmware
     target can reuse an existing replay cache

Default: `1`

### `REPLAY_VERSION=<version>`

Select the version component used for the rendered firmware validation profile.
When `REPLAY_INPUT` is unset and the wrapper downloads a release package, the
resolved release tag overrides this value for that run.

Default: `1.3.1`

### `REPLAY_BUILD_OPTIONS=<path>`

Provide the `BuildOptions` file when `REPLAY_INPUT` points directly at a raw
`cix_flash_all.bin`.

Default: unset

### `REPLAY_BUILD_DATE=<iso8601>`

Provide a fallback firmware build timestamp when replay inputs do not include
`BuildOptions`.

Default: unset

## Build Cache Variables

These variables affect build speed, not firmware features.

When `ccache` is available in the build environment, the firmware build wraps
the EDK2 cross compiler with it automatically. If it is not available, the
build prints a warning and continues without compiler-cache acceleration.

### `CCACHE_DISABLE=1`

Set the standard ccache kill switch to disable compiler-cache use. Leave it
unset to use ccache opportunistically when it is available.

Default: unset

### `CCACHE_DIR=<path>`

Set the managed compiler-cache directory.

Default when running the rendered firmware tree directly:
`$(REPO_ROOT)/build-cache/ccache`

Default when invoking the firmware build through the `build` branch:
`.cache/edk2-cix/firmware/ccache` inside the rendered worktree.

### `CCACHE_WRAPPER_ROOT=<path>`

Set the directory used for generated compiler-wrapper symlinks.

Default when running the rendered firmware tree directly:
`$(REPO_ROOT)/build-cache/ccache-toolchain`

Default when invoking the firmware build through the `build` branch:
`.cache/edk2-cix/firmware/ccache-toolchain` inside the rendered worktree.

### `CCACHE_BIN=<path>`

Override the `ccache` executable name or path. For buildbox targets, this path
is resolved inside the buildbox container.

Default: `ccache`

## Vendor trusted firmware

`CIX_RELEASE=1.2` (also `v1.2`) is a custom-only opt-in. It selects the pinned
CIX 2026Q1 BL1 and compiles BL31 and OP-TEE from the curated V1.2 sources.
The reviewed selection currently supports O6 with Radxa 1.3.1 source
checkpoints at EDK2 `202208`, `202605` and `202608`; other boards and source
checkpoints fail preflight.
The trusted FIP is signed with [CIX-published keys](firmware-chain-validation.md)
whose root public key matches the selected stock firmware's trusted-world
anchor. The build rejects changed keys, a bad certificate chain, changed BL1,
rollback counters below the vendor reference, or an oversized flash payload.

Leave `CIX_RELEASE` unset or empty to retain the selected vendor BL1, BL31
and OP-TEE. A `/cix-1.2/` segment in `RELEASE` identifies imported source
lineage; it does not itself enable source-built trusted components.
`ENABLE_TF_A_FIXES=true` separately opts into the curated TF-A fixes when
`CIX_RELEASE=1.2` is active. A certificate-valid image still needs board
qualification; eFuse, rollback state and runtime compatibility are not
observable by the offline validator.

## Opt-In Firmware Behaviour Changes

These switches change what your built firmware exposes to the operating system
or to the setup UI.

### `ENABLE_FIRMWARE_FIXES=true|false`

To apply the opt-in firmware fixes and enhancements described in
[`FEATURES.md`](features.md), set this variable.

Example:

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  ENABLE_FIRMWARE_FIXES=true
```

When you enable it, the firmware metadata and behaviour change in meaningful
ways. This is not just a cosmetic tweak. The current scope includes:

- ACPI and reserved-memory fixes
- PCIe `_OSC` cleanup
- the PCIe device-model selector
- DSU PMU exposure
- SMBIOS and PPTT improvements
- display metadata cleanup

See [`FEATURES.md`](features.md) for the detailed feature list and rationale.

This setting is only valid with:

- `ARTEFACT_MODE=custom`
- `FIRMWARE_BOARD=O6` or `FIRMWARE_BOARD=O6N`

Default: `false`

### `ENABLE_CORE_ORDER=cix|conventional|performance`

This setting only has an effect when `ENABLE_FIRMWARE_FIXES=true` is also
enabled.

- `cix`
   - preserve the vendor/default exposed CPU UID order
   - this is also the effective behaviour when `ENABLE_CORE_ORDER` is unset
- `conventional`
   - remap exposed CPU UIDs to a little-cores-first layout
   - this is the most conventional heterogeneous-core presentation
- `performance`
   - remap exposed CPU UIDs so the A720 cores appear before the A520 cores,
    starting with the highest-performance A720 pair
   - useful for experiments, but more likely to surprise tools and users that
    expect little-first numbering

This changes the exposed CPU numbering seen by the OS. It does not change the
physical hardware topology itself.

`conventional` and `performance` require `ARTEFACT_MODE=custom` and
`ENABLE_FIRMWARE_FIXES=true` on O6 or O6N. An explicit `cix` is also accepted
with fixes disabled because it preserves the default order.

Default: unset, which behaves like `cix`

### `ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true|false`

To enable the separate experimental Radxa setup overlay, set this variable.

Example:

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true
```

This adds additional user-facing setup controls while keeping the Radxa setup
stack in place, rather than switching over to the broader CIX setup path.

Current examples include:

- RTC wakeup
- selected power-control options
- serial console handoff settings
- setup console-size selection
- SR-IOV on `O6`

This setting is only valid with:

- `ARTEFACT_MODE=custom`
- `FIRMWARE_BOARD=O6` or `FIRMWARE_BOARD=O6N`

It is designed to coexist with `ENABLE_FIRMWARE_FIXES=true`.

Default: `false`

## Serial and Debug Variables

These switches mostly affect where firmware logs go and how much firmware debug
output you see.

### `UART3_ENABLE=true|false`

To enable the UART3 header path without moving firmware `DEBUG()` output there,
set this variable.

When enabled on the custom path:

- UART3 is muxed for UART use instead of GPIO
- UART3 is exposed to the OS through firmware metadata
- firmware `DEBUG()` output still stays on UART2 unless you also set
  `DEBUG_ON_UART3=true`

This consumes header GPIO105 and GPIO106 while active, so those lines are no
longer available as general GPIO.

This setting is only valid with:

- `ARTEFACT_MODE=custom`

Default: `false`

### `DEBUG_ON_UART3=true|false`

To route firmware `DEBUG()` output to UART3 instead of UART2 on the custom
path, set this variable.

When enabled:

- firmware `DEBUG()` output moves to UART3
- `UART3_ENABLE=true` is implied automatically

This gives you a dedicated firmware-debug serial channel on UART3.

This setting is only valid with:

- `ARTEFACT_MODE=custom`

Default: unset

### `DEBUG_VERBOSE=true|false`

To turn `DEBUG()` logging back on for a `RELEASE` build without switching to a
full `DEBUG` build, set this variable.

When enabled on the custom path:

- `RELEASE` builds emit firmware `DEBUG()` logs again
- `MDEPKG_NDEBUG` and `NDEBUG` remain defined: assertions and source guarded
  by those definitions keep their normal RELEASE behavior
- `DEBUG_CODE` blocks and debug memory filling are compiled out; logging
  inside an excluded debug block remains excluded too
- if `DEBUG_PRINT_ERROR_LEVEL` is left unset, the build uses the full known
  `DEBUG_*` message mask by default

This enables logging without the other code normally enabled by a `DEBUG`
build. It does not change `FIRMWARE_TARGET=DEBUG`, optimization settings, or
the flash layout. Log strings, argument calculations, and print calls still
consume space, so logging-only builds can still exceed the firmware limit.

For targeted setup-migration and BDS diagnostics, use `DEBUG_VERBOSE=false`
with `DEBUG_PRINT_ERROR_LEVEL=0x80000001`. Alternatively, verbose RELEASE can
select individual categories, or leave the mask unset for all categories.
The actual signed FIP determines whether the original slot suffices or
`DEBUG_ALLOW_LARGE_IMAGE=1` is required. Final size/signature checks cannot
be bypassed.

This setting is only valid with:

- `ARTEFACT_MODE=custom`

Default: `false`

### `DEBUG_PRINT_ERROR_LEVEL=<u32>`

To override the firmware debug message mask directly, set this variable.

- accepts decimal or `0x`-prefixed 32-bit values
- use `make help-debug` or `make -C src help-debug` to see the accepted bit
  names and values derived from `DebugLib.h`

On the custom path:

- the default mask is `0x80000040` (`DEBUG_INFO|DEBUG_ERROR`)
- if `DEBUG_VERBOSE=true` is set and this variable is left unset, the build
  enables all known `DEBUG_*` message classes by default

This setting is only valid with:

- `ARTEFACT_MODE=custom`

Default: unset

### `DEBUG_ALLOW_LARGE_IMAGE=0|1`

Non-default `DEBUG_PRINT_ERROR_LEVEL` values can retain more logging and increase
image size. `DEBUG_VERBOSE=true` also selects every category when no mask is
given. If the resulting signed BL33 FIP exceeds its original slot, the build
fails unless `DEBUG_ALLOW_LARGE_IMAGE=1` permits the audited extension.

Permit the audited larger BL33 slot for a custom build whose signed FIP exceeds
the original allocation. A small image retains its original layout even with
this option. A large image emits `cix_flash_all.bin` and no OTA image. Images
beyond the physical flash or FD RAM limits fail with either value.

Undefined mask bits always fail. Valid selected categories without labelled
source diagnostics produce a warning; the build retains the requested bits.
No predictive mask-size whitelist is used. See [Debug](debug.md).

Default: `0`. Only supported with `ARTEFACT_MODE=custom`.

`FORCE_DEBUG_BUILD=0|1` remains accepted as an inert compatibility input. It
neither authorises an enlarged layout nor bypasses validation.

### `O6_SMBIOS_ASSET_TAG=<text>`

To set both O6 SMBIOS asset-tag fields together on the custom path, set this
variable.

When set:

- SMBIOS Type 2 `Asset Tag` uses this value
- SMBIOS Type 3 `Asset Tag` uses this value
- a more specific override variable wins if you also set one

This setting is only valid with:

- `ARTEFACT_MODE=custom`
- `FIRMWARE_BOARD=O6`

Leaving it unset keeps both asset-tag fields absent (`0` / no string).

### `O6_SMBIOS_BASEBOARD_ASSET_TAG=<text>`

To override only the O6 SMBIOS Type 2 baseboard asset tag on the custom path,
set this variable.

This setting is only valid with:

- `ARTEFACT_MODE=custom`
- `FIRMWARE_BOARD=O6`

Leaving it unset falls back to `O6_SMBIOS_ASSET_TAG`, and if both are unset the
Type 2 asset-tag field stays absent.

### `O6_SMBIOS_CHASSIS_ASSET_TAG=<text>`

To override only the O6 SMBIOS Type 3 chassis asset tag on the custom path, set
this variable.

This setting is only valid with:

- `ARTEFACT_MODE=custom`
- `FIRMWARE_BOARD=O6`

Leaving it unset falls back to `O6_SMBIOS_ASSET_TAG`, and if both are unset the
Type 3 asset-tag field stays absent.

### `V=0|1`

To change build verbosity, set this variable.

- `V=0`
   - the default
   - keeps the higher-level progress output without dumping every raw EDK2
    command
- `V=1`
   - show raw EDK2 command lines

Default: `0`

### `DEBUG=0|1`

Tooling diagnostics: enable Python tracebacks for unexpected tool failures and
unfiltered EDK2 diagnostic output. This does not select `FIRMWARE_TARGET=DEBUG`
or turn on firmware `DEBUG_VERBOSE` logging. `V=1` additionally requests raw
build commands. See the [warning policy](build.md#compiler-warnings).

Default: `0`

## Compatibility Summary

The most important compatibility rules are:

- `PROFILE=upstream` and `ARTEFACT_MODE=upstream` reject active custom-only
  feature variables; explicit false boolean gates are harmless
- `ENABLE_CORE_ORDER=conventional|performance` requires `ENABLE_FIRMWARE_FIXES=true`;
  `cix` preserves the default order and is accepted with fixes disabled
- `DEBUG_ON_UART3=true` implies `UART3_ENABLE=true`
- `CIX_RELEASE=1.2|v1.2` requires `ARTEFACT_MODE=custom`
- the `O6_SMBIOS_*` asset-tag variables are custom-only and board-limited to
  `O6`
- `ENABLE_FIRMWARE_FIXES=true` and `ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true` are
  designed to coexist

## Common Recipes

### Byte-identical vendor release replay

```bash
make
make deterministic-replay FIRMWARE_BOARD=O6
make deterministic-replay FIRMWARE_BOARD=O6N
```

### Latest maintained source without opinionated fixes

```bash
make PROFILE=latest
```

### Latest maintained source with opinionated fixes

```bash
make PROFILE=latest ENABLE_FIRMWARE_FIXES=true
```

### Closest-to-upstream vendor-path build

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=upstream \
  FIRMWARE_BOARD=O6
```

### Custom build with firmware fixes

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  ENABLE_FIRMWARE_FIXES=true
```

### Custom build with fixes plus the conventional little-first CPU order

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  ENABLE_FIRMWARE_FIXES=true \
  ENABLE_CORE_ORDER=conventional
```

### Custom build with the experimental setup overlay as well

```bash
make build \
  RELEASE=edk2-202608/radxa-1.3.1/unofficial \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  ENABLE_FIRMWARE_FIXES=true \
  ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true
```

### Curated CIX V1.2 trusted components

```bash
make build \
  RELEASE=edk2-202608/radxa-1.3.1/unofficial \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  FIRMWARE_TARGET=RELEASE \
  FIRMWARE_DISTRO=trixie \
  ENABLE_FIRMWARE_FIXES=true \
  ENABLE_EXPERIMENTAL_UEFI_SETTINGS=false \
  CIX_RELEASE=1.2
```

CI compiles both TF-A fix configurations and validates their signed trusted
FIPs. A full image must also pass the existing BL1, certificate-chain and
flash-layout guards before publication. See
[certificate-chain validation](firmware-chain-validation.md).

### Experimental RELEASE build with verbose firmware logs on UART3

This permits the audited full-image layout when needed; final bounds and
signature checks remain mandatory, and testing on hardware remains separate.

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  DEBUG_VERBOSE=true \
  DEBUG_ALLOW_LARGE_IMAGE=1 \
  DEBUG_ON_UART3=true
```

## Rebuild recipe in System Information

Custom builds with `ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true` append **Rebuild
running firmware** to the existing **System Information** page, below its
component versions and source revisions. No separate menu is added. It records the source
release, build checkout, board, target, distribution, firmware fixes,
core order, CIX selection, TF-A fixes, experimental settings, UART routing,
verbose logging, debug mask, large-image consent and nonempty metadata overrides. Empty values are
shown explicitly, including `CIX_RELEASE=''`. These are compile-time values;
changing setup variables does not alter the recipe.

The same command and configuration are saved as `firmware-rebuild.txt` and
`firmware-rebuild.json` beside the output firmware. Preserve the recorded build
checkout and required source refs. Matching arguments reconstruct the build
configuration; certificate timestamps and host-tool differences can still affect
byte identity. The generated HII files live in the private custom workspace and
never overwrite imported source files. This facility does not run in upstream
mode. Direct source-tree builds must supply `FIRMWARE_REBUILD_RELEASE` with their
source-target label; public build commands supply it automatically.

### DEBUG size and tokenized logging

The inspected Radxa 1.3.1 DEBUG configuration enables its serial debug library
and debug code, subject to its `0x80000040` print mask and per-module library
bindings. It does not enable every message category. No separate vendor method
for fitting unrestricted DEBUG output in the production flash layout has been
identified. DEBUG increases code and assertion data as well as message strings.
The compressed firmware volume already uses LZMA.

Custom builds measure the compressed FV and adjust the FD within the flash and
RAM limits described in [BL33 sizing](debug-layout.md). This applies to both
RELEASE and DEBUG; a source FDF's fixed 4 MiB DEBUG allowance is not evidence
that the contents need that much space. `DEBUG_ALLOW_LARGE_IMAGE=1` can permit
the audited 1.3.1 slot extension, but cannot waive the final signed-FIP bounds.
Full DEBUG still includes additional code and assertions and has not inherited
the logging-only RELEASE qualification.

Replacing debug formats with token IDs and an external dictionary is feasible,
but is not implemented here. A useful implementation must encode argument types
and values, handle EDK2-specific formats such as `%r`, `%g`, `%a` and `%s`, retain
a dictionary for the exact build, and work before allocation services exist.
Assertions, dynamic strings and early-boot failure paths need separate treatment.
Measure the final compressed size before relying on tokenization to make DEBUG
flashable; message text is not its only additional cost.
