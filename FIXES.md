# Firmware Fixes Bundle

`main-monorepo` now supports an opt-in firmware fixes bundle for custom builds:

```bash
make -C src O6 ARTEFACT_MODE=custom ENABLE_FIRMWARE_FIXES=true
```

This bundle is intentionally separate from the default custom build so that we
can stage a set of higher-impact firmware changes together without changing the
baseline image for every user immediately.

## Scope

`ENABLE_FIRMWARE_FIXES=true` is only valid with `ARTEFACT_MODE=custom`.

It is also designed to coexist with:

```bash
make -C src O6 \
  ARTEFACT_MODE=custom \
  ENABLE_FIRMWARE_FIXES=true \
  ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true
```

When the bundle is enabled, firmware applies the following fixes and
enhancements inspired by the investigation against stock firmware and the
Unlocked tree:

- Audio DMA reserved-memory regions are marked as reserved in PEI, matching the
  ACPI description that Linux already consumes.
- IORT SMMUv3 nodes advertise HTTU override support instead of forcing Linux to
  assume it is absent.
- PCIe `_OSC` only keeps SHPC masked, allowing the OS to retain PME, AER, and
  LTR ownership.
- A DSU PMU ACPI device is exposed so Linux can discover the shared cluster PMU
  path that is missing in stock firmware.
- Display ACPI metadata only emits the `edp-panel` property for the actual eDP
  path instead of attaching empty-string placeholders to non-eDP outputs.
- A new UEFI PCIe selector chooses which ACPI PCIe device model firmware
  exposes: standard Linux `PNP0A08` root bridges or vendor-style `CIXH2020`
  Cadence devices.
- PPTT gains a conservative cache topology model instead of exposing processor
  nodes without cache information.

## User-visible Behaviour

### PCIe device model selector

When the fixes bundle is enabled, the PCIe setup page adds:

- `Linux (PNP0A08)`
- `CIX (CIXH2020)`

`Linux (PNP0A08)` is the default and is the safest choice for upstream kernels.
It exposes standard PCI root bridges and hides the vendor-specific Cadence
devices.

`CIX (CIXH2020)` exposes the vendor-style Cadence ACPI devices instead and
hides the standard root bridges. This is intended for kernels that use the CIX
/ Cadence PCIe host driver. Some vendor kernels may require an initramfs step
that rescans PCIe before the NVMe root device becomes visible.

### Conservative PPTT topology

The PPTT update uses the information currently available from public platform
documentation, SMBIOS, and runtime investigation:

- A520 cores: `32 KiB` L1I + `32 KiB` L1D
- A720 cores: `64 KiB` L1I + `64 KiB` L1D
- A720 cores: private `512 KiB` L2
- Shared `12 MiB` L3

The little-core A520 L2 arrangement is still not fully confirmed. To avoid
inventing a possibly wrong shared/private L2 relationship, the conservative
model does **not** currently describe an A520 L2 cache. That is deliberate: it
is better to under-describe the uncertain part than to encode the wrong shared
cache topology.

## What Is Not Included Yet

Two investigated areas are intentionally still outside this bundle pending more
runtime confirmation from end users:

- `ramoops` / persistent crash log behaviour
- USB reboot state handling

Those can be revisited later without needing to reshape the initial
`ENABLE_FIRMWARE_FIXES` bundle.

## Build Discovery

To see the new make variable in the build help:

```bash
make -C src help-vars
```

The help text there describes the bundle as opt-in and custom-only.
