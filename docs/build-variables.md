# Build Variables

This page explains what each user-facing build variable changes on your O6 or
O6N system. The short `make help-vars` output is a quick reminder; this
document covers the side effects, compatibility rules, and typical use of each
variable.

## The Main Build Choices

Start with these variables.

### `ARTEFACT_MODE=custom|upstream`

This is the highest-level build-mode switch.

- `ARTEFACT_MODE=upstream`
  - keep the upstream vendor build path
  - this is the right choice for closest-to-upstream diagnostics on
    `source/unofficial/<line>/current`
  - byte-identical replay of the published 202208-based vendor releases belongs
    on `source/unofficial/edk2-stable202208`, not on the rebased EDK2 branch
  - custom-only feature switches are rejected in this mode
- `ARTEFACT_MODE=custom`
  - keep the same overall build flow, but allow the local overlays, source
    replacements, and opt-in firmware changes
  - this is the right choice for local overlays and opt-in firmware changes
    instead of an upstream-identical vendor image

Default: `custom`

`ARTEFACT_MODE=upstream` is the mode that follows the upstream vendor build
path. On `source/unofficial/<line>/current`, this path uses the rebased upstream EDK2
implementation, so it deliberately does not claim byte-for-byte equivalence
with the older published vendor images.

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
  - enables the broader EDK2 debug/assert behavior expected from a DEBUG build

Default: `RELEASE`

### `FIRMWARE_DISTRO=bookworm|trixie`

Select the default distro family used by the buildbox helpers and
buildbox helpers. This is an advanced override; it is intentionally not shown in
the short `make help-vars` output on `source/unofficial/<line>/current`.

- `bookworm`
  - still supported for compatibility checks
  - emits a warning in buildbox preflight because it is no longer the branch
    default
- `trixie`
  - the default for all `source/unofficial/<line>/current` buildbox firmware builds,
    including `ARTEFACT_MODE=upstream`
  - the preferred Debian/toolchain family for the rebased EDK2 implementation

This does not change the firmware feature set directly. It changes the build
environment used by the wrapper targets.

Default: `trixie`

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

Default: `$(REPO_ROOT)/build-cache/ccache`

### `CCACHE_WRAPPER_ROOT=<path>`

Set the directory used for generated compiler-wrapper symlinks.

Default: `$(REPO_ROOT)/build-cache/ccache-toolchain`

### `CCACHE_BIN=<path>`

Override the `ccache` executable name or path. For buildbox targets, this path
is resolved inside the buildbox container.

Default: `ccache`

## Curated CIX Inputs

### `CIX_RELEASE=v1.2`

Set this on the custom build path to select the curated CIX early-boot path.

Example:

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  CIX_RELEASE=v1.2
```

When you enable it, the build:

- imports the public CIX BIOS V1.2 TF-A and OP-TEE source set
- stages the later public CIX community-release `bootloader1.img` payload that
  matches community hardware logs
- source-builds `bootloader2.img` during the packaging step

This is intentionally a curated mode, not an exact historical replay of one
public superproject commit. The public CIX source and release lineage is
useful, but it is not represented by one clean public tag or repo snapshot.

This setting is only valid with:

- `ARTEFACT_MODE=custom`
- `FIRMWARE_BOARD=O6` or `FIRMWARE_BOARD=O6N`

It can coexist with:

- `ENABLE_FIRMWARE_FIXES=true`
- `ENABLE_TF_A_FIXES=true`
- `ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true`

Default: unset

### `ENABLE_TF_A_FIXES=true|false`

Set this with `CIX_RELEASE=v1.2` on the custom build path to compile custom
fixes into the source-built TF-A BL31 image. The current fix suppresses the
noisy TF-A mailbox `NOTICE` emitted for `FFA_GET_FUSE_BY_ID` commands while
leaving notices for other mailbox commands intact.

Example:

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  CIX_RELEASE=v1.2 \
  ENABLE_TF_A_FIXES=true
```

This setting is only valid with `ARTEFACT_MODE=custom`. If you set
`ENABLE_TF_A_FIXES=true` without `CIX_RELEASE=v1.2`, the build emits a
non-fatal warning and the setting has no effect because TF-A is not
source-built in that configuration.

Default: `false`

## Opt-In Firmware Behavior Changes

These switches change what your built firmware exposes to the operating system
or to the setup UI.

### `ENABLE_FIRMWARE_FIXES=true|false`

To apply the opt-in firmware fixes described in [`FIXES.md`](../FIXES.md), set this variable.

Example:

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  ENABLE_FIRMWARE_FIXES=true
```

When you enable it, the firmware metadata and behavior change in meaningful
ways. This is not just a cosmetic tweak. The current scope includes:

- ACPI and reserved-memory fixes
- PCIe `_OSC` cleanup
- PCIe resource-window and ECAM-reservation repairs
- PCIe SMMUv3/IORT restoration on the 1.3 firmware line
- the PCIe device-model selector
- the USB device-model selector
- GPU cache-coherency metadata correction
- thermal-zone metadata and EC critical-trip repairs
- visible MTE capacity warnings
- DSU PMU exposure
- SMBIOS and PPTT improvements
- display metadata cleanup

See [`FIXES.md`](../FIXES.md) for the detailed list and rationale.

This setting is only valid with:

- `ARTEFACT_MODE=custom`
- `FIRMWARE_BOARD=O6` or `FIRMWARE_BOARD=O6N`

Default: `false`

### `ENABLE_CORE_ORDER=cix|conventional|performance`

This setting only has an effect when `ENABLE_FIRMWARE_FIXES=true` is also
enabled.

- `cix`
  - preserve the vendor/default exposed CPU UID order
  - this is also the effective behavior when `ENABLE_CORE_ORDER` is unset
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

This setting is only valid with:

- `ARTEFACT_MODE=custom`
- `ENABLE_FIRMWARE_FIXES=true`
- `FIRMWARE_BOARD=O6` or `FIRMWARE_BOARD=O6N`

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
- PCIe ACPI model controls when combined with `ENABLE_FIRMWARE_FIXES=true`
- selected power-control options
- serial console handoff settings
- setup console-size selection
- USB ACPI model and Type-C visibility controls when combined with
  `ENABLE_FIRMWARE_FIXES=true`
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
- if `DEBUG_PRINT_ERROR_LEVEL` is left unset, the build uses the full known
  `DEBUG_*` message mask by default

This gives you substantially more firmware logging without switching the whole
image to a `DEBUG` build.

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
  - the curated `CIX_RELEASE=v1.2` helper also keeps its TF-A and OP-TEE
    sub-build output quiet on success in this mode
- `V=1`
  - show raw EDK2 command lines
  - pass through the raw vendor sub-build output for the curated CIX V1.2
    helper as well

Default: `0`

## Compatibility Summary

The most important compatibility rules are:

- `ARTEFACT_MODE=upstream` rejects all custom-only feature variables
- `ENABLE_CORE_ORDER=...` requires `ENABLE_FIRMWARE_FIXES=true`
- `DEBUG_ON_UART3=true` implies `UART3_ENABLE=true`
- `CIX_RELEASE=v1.2` is custom-only and board-limited to `O6` / `O6N`
- `ENABLE_TF_A_FIXES=true` is custom-only and only affects builds that also
  set `CIX_RELEASE=v1.2`
- the `O6_SMBIOS_*` asset-tag variables are custom-only and board-limited to
  `O6`
- `ENABLE_FIRMWARE_FIXES=true` and
  `ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true` are designed to coexist

## Common Recipes

### Closest-to-upstream build

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
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  ENABLE_FIRMWARE_FIXES=true \
  ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true
```

### Custom build using the curated CIX V1.2 early-boot path

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  CIX_RELEASE=v1.2
```

### RELEASE build with verbose firmware logs on UART3

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  DEBUG_VERBOSE=true \
  DEBUG_ON_UART3=true
```
