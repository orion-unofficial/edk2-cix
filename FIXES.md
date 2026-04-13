# Firmware Fixes Bundle

`main-monorepo` now supports an opt-in firmware fixes bundle for custom builds:

```bash
make -C src FIRMWARE_BOARD=O6 ARTEFACT_MODE=custom ENABLE_FIRMWARE_FIXES=true
```

This bundle is intentionally separate from the default custom build so that we
can stage a set of higher-impact firmware changes together without changing the
baseline image for every user immediately.

## Scope

`ENABLE_FIRMWARE_FIXES=true` is only valid with `ARTEFACT_MODE=custom`.

The optional CPU UID ordering switch is only active inside the same gated
surface:

```bash
make -C src FIRMWARE_BOARD=O6 \
  ARTEFACT_MODE=custom \
  ENABLE_FIRMWARE_FIXES=true \
  ENABLE_CORE_ORDER=conventional
```

It is also designed to coexist with:

```bash
make -C src FIRMWARE_BOARD=O6 \
  ARTEFACT_MODE=custom \
  ENABLE_FIRMWARE_FIXES=true \
  ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true
```

When the bundle is enabled, firmware applies the following fixes and
enhancements inspired by investigation against stock firmware and the Unlocked
tree.

The entries below are ordered by likely user impact rather than by the order in
which they were discovered or implemented.

## Included Fixes

### Audio DMA reserved-memory alignment

The stock firmware ACPI tables describe several audio/DMA regions as reserved,
but PEI did not reserve those same ranges before handing memory to later boot
stages.

When the fixes bundle is enabled, PEI now reserves the DSP, DMA1, and HDA audio
buffers so that the runtime memory map matches the ACPI metadata Linux already
consumes.

This is one of the highest-impact correctness fixes in the bundle because it
stops firmware from describing memory as reserved while still leaving it
available to the general allocator.

### PCIe `_OSC` ownership handoff

The stock PCIe `_OSC` implementation keeps too much control in firmware and
denies the OS ownership of capabilities that Linux expects to manage itself.

With the fixes bundle enabled, firmware only keeps SHPC masked and allows the
OS to retain PME, AER, and LTR ownership.

This improves standards compliance and makes the ACPI PCIe model closer to what
upstream kernels expect from normal host bridges.

### PCIe device-model selector

When the fixes bundle is enabled, the PCIe setup page adds a selector for the
ACPI PCIe model that firmware exposes:

- `Linux (PNP0A08)`
- `CIX (CIXH2020)`

`Linux (PNP0A08)` is the default and is the safest choice for upstream kernels.
It exposes standard PCI root bridges and hides the vendor-style Cadence ACPI
devices.

`CIX (CIXH2020)` exposes the vendor-style Cadence ACPI devices instead and
hides the standard root bridges. This is intended for kernels that use the CIX
/ Cadence PCIe host driver. Some vendor kernels may still require an initramfs
step that rescans PCIe before the NVMe root device becomes visible.

This setting exists because the two ACPI PCIe models are mutually exclusive in
practice on Linux, and exposing both at once is not a stable long-term answer.

### `_CPC` nominal and lowest frequency repair

The stock static ACPI `_CPC` update path treats SCMI performance levels as if
they were already expressed in MHz. In practice that can publish wrong nominal
and lowest frequency integers even when the performance levels themselves are
correct.

With the fixes bundle enabled, firmware now derives the CPPC frequency scaling
from SCMI domain attributes instead of assuming a fixed `1 MHz` granularity.

This keeps the static DSDT CPPC view aligned with the same SCMI performance
model that the dynamic-table path already understood more accurately.

### Optional CPU UID ordering modes

By default, the fixes bundle preserves the existing CIX/vendor CPU UID order:

- `ENABLE_CORE_ORDER=cix`

That is also the effective behavior when `ENABLE_CORE_ORDER` is not set at all.

Two alternative UID-ordering modes are available inside the same gated surface.

When a user enables:

- `ENABLE_CORE_ORDER=conventional`

firmware remaps the exposed CPU UIDs to the more conventional
little-cores-first ordering while leaving the physical-core keyed policy data
such as `_PSD` and `_CPC` attached to the same underlying cores.

This makes the Linux-visible CPU numbering easier to reason about on a
heterogeneous desktop/workstation system without forcing the default custom
build away from the current vendor-visible layout.

When a user instead enables:

- `ENABLE_CORE_ORDER=performance`

firmware remaps the exposed CPU UIDs to a highest-performance-first ordering
inspired by the same policy used by the Unlocked tree.

This mode is intentionally opt-in because it is more likely to surprise users
and tooling that expect a conventional little-first heterogeneous layout, but
it is useful for users who explicitly want the fastest clusters to appear at
the lowest CPU numbers.

### IORT HTTU override

The stock IORT SMMUv3 nodes do not advertise HTTU override support, which leads
Linux to assume that the override path is absent and to log that the effective
HTTU state is forced down.

With the fixes bundle enabled, firmware advertises HTTU override support in the
SMMUv3 nodes instead of forcing Linux to fall back to the more limited default
view.

This is a targeted metadata fix: small on the firmware side, but important for
accurate IOMMU capability description.

### `ramoops` reserved-memory alignment

The ACPI tables already describe a reserved memory window for the `ramoops`
device, but PEI did not reserve that same window in the memory handoff.

With the fixes bundle enabled, firmware now reserves the ACPI-described
`ramoops` range in PEI so the reserved-memory story is internally consistent.

This does **not** guarantee that Linux will prefer `ramoops` over `efi_pstore`
on every system, but it does remove one obvious firmware-side mismatch that
could otherwise make the `ramoops` path unreliable.

### SMBIOS OEM hook and cache stability fixes

Three SMBIOS-related issues are corrected when the fixes bundle is enabled.

First, the CIX OEM SMBIOS hooks no longer return an `EFI_STATUS` value through
a `BOOLEAN` API on failure paths. In the stock code that can collapse some
errors into a truthy value instead of a clean `FALSE`.

Second, the package-level SMBIOS cache totals are no longer tied to which core
booted first. The fixes bundle switches that path to a stable topology-driven
model so the reported cache totals reflect the SoC layout rather than a
boot-core accident.

Third, the O6 board-specific SMBIOS producer no longer emits its hard-coded
Type `4` and Type `7` CPU/cache records when the fixes bundle is enabled. That
leaves processor/cache ownership with the common CIX/Arm SMBIOS path while
keeping the Radxa board/system/chassis records in place.

In practice this means the fixes-enabled O6 image now follows the cleaner split
we want long-term:

- Cix describes the SoC / CPU / cache identity
- Radxa describes the board / system / chassis identity

### DSU PMU ACPI exposure

The stock firmware does not expose the DSU PMU through ACPI, which means Linux
cannot discover the shared cluster PMU path even though the SoC uses a DSU/L3
arrangement where that information matters.

With the fixes bundle enabled, firmware exposes the DSU PMU ACPI device so that
Linux can discover the shared cluster PMU path that is missing in stock
firmware.

This is mainly a diagnostics and observability improvement, but it is a real
hardware-description fix rather than a cosmetic change.

### Conservative PPTT cache topology

The stock PPTT path exposed processor hierarchy without a usable cache
topology. That left the OS with CPU nodes but little or no trustworthy cache
information.

With the fixes bundle enabled, PPTT uses a conservative cache model derived
from public platform documentation, SMBIOS, and runtime investigation:

- A520 cores: `32 KiB` L1I + `32 KiB` L1D
- A720 cores: `64 KiB` L1I + `64 KiB` L1D
- A720 cores: private `512 KiB` L2
- Shared `12 MiB` L3

The little-core A520 L2 arrangement is still not fully confirmed. To avoid
inventing a possibly wrong shared/private L2 relationship, the conservative
model does **not** currently describe an A520 L2 cache. That is deliberate: it
is better to under-describe the uncertain part than to encode the wrong shared
cache topology.

### `edp-panel` ACPI property cleanup

The stock display ACPI metadata attaches the `edp-panel` property to non-eDP
display paths with empty-string placeholder values.

With the fixes bundle enabled, firmware only emits the `edp-panel` property for
the actual eDP path instead of attaching empty-string placeholders to DP/UCP
outputs that are not panels.

This is a smaller fix than the items above, but it corrects a real firmware
metadata bug and makes the display topology description less misleading.

## What Is Not Included Yet

Two investigated areas are still intentionally outside the bundle, or only
partially addressed, pending more runtime confirmation from end users:

- The `ramoops` memory-window mismatch is fixed, but the final Linux backend
  choice and end-to-end persistence behaviour still need more confirmation on
  real systems.
- USB reboot state handling is still not changed by this bundle.

Those can be revisited later without needing to reshape the current
`ENABLE_FIRMWARE_FIXES` surface.

## Build Discovery

To see the new make variable in the build help:

```bash
make -C src help-vars
```

The help text there describes the bundle as opt-in and custom-only.
