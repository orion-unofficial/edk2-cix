# Firmware Fixes

This repository can either follow the upstream vendor firmware path, including
byte-identical rebuilds when you use `ARTEFACT_MODE=upstream` with the
published replay inputs, or build a custom image with additional fixes
applied. To build the custom image with the fixes described below, run:

```bash
make ARTEFACT_MODE=custom ENABLE_FIRMWARE_FIXES=true
```

The short variable reference is also available from `make help-vars` or
`make -C src help-vars` when you have the repo checked out, but this page is
intended to stand on its own.

To stay on the upstream vendor path instead, set `ARTEFACT_MODE=upstream` and
leave `ENABLE_FIRMWARE_FIXES` unset. The fixes below only apply on the custom
build path.

## Included Fixes

### Audio DMA Reserved-Memory Alignment

Stock ACPI tables describe several audio and DMA buffers as reserved, but PEI
(Pre-EFI Initialization, the early firmware phase that discovers memory and
hands it to later stages) did not reserve those same ranges.

With `ENABLE_FIRMWARE_FIXES=true`, PEI reserves the DSP, DMA1, and HDA audio
buffers so that the runtime memory map matches the ACPI metadata Linux already
uses.

### PCIe Capability Handoff (`_OSC`)

`_OSC` is the ACPI “Operating System Capabilities” handshake for PCIe. It tells
the firmware and the operating system which side owns features such as hot-plug
and error reporting.

Stock firmware keeps too much control in firmware. With
`ENABLE_FIRMWARE_FIXES=true`, firmware only keeps SHPC (Standard Hot-Plug
Controller) masked and lets the OS own PME (Power Management Events), AER
(Advanced Error Reporting), and LTR (Latency Tolerance Reporting).

This makes the PCIe description closer to what upstream kernels expect from a
normal host-bridge implementation.

### PCIe Device-Model Selector

With `ENABLE_FIRMWARE_FIXES=true`, the PCIe setup page adds a selector for the
ACPI PCIe model the firmware exposes:

- `Linux (PNP0A08)`
- `CIX (CIXH2020)`

`Linux (PNP0A08)` is the safer default for upstream kernels. It exposes
standard PCI root bridges and hides the vendor-style Cadence ACPI devices.

`CIX (CIXH2020)` exposes the vendor-style Cadence ACPI devices instead and
hides the standard root bridges. Select it when you are deliberately booting a
kernel that uses the CIX / Cadence PCIe host driver.

Some vendor kernels may still require an initramfs step that rescans PCIe
before any NVMe or other PCIe-connected root device becomes visible. This
setting exists because the two ACPI PCIe models are mutually exclusive in
practice on Linux, and exposing both at once is not a stable long-term answer.

### CPU Performance Description Repair (`_CPC` / CPPC)

The ACPI `_CPC` objects are part of CPPC (Collaborative Processor Performance
Control). They describe CPU performance levels to the operating system.

Stock firmware treats SCMI (System Control and Management Interface)
performance levels as if they were already expressed in MHz. With
`ENABLE_FIRMWARE_FIXES=true`, the firmware derives the CPPC frequency scaling
from the SCMI domain attributes instead.

That keeps the static ACPI CPU-performance view aligned with the same SCMI
performance model the firmware is already using underneath.

### Optional CPU Numbering Modes

If you also set `ENABLE_CORE_ORDER` to one of the two additional modes below,
the firmware can change the CPU UIDs it exposes to the operating system. This
does not change the physical core layout. It only changes the numbering the OS
sees.

The available modes are:

- `cix`
  - keep the vendor/default CPU numbering
- `conventional`
  - expose CPUs with the "LITTLE" A520 cores before the A720 cores
- `performance`
  - expose CPUs with the A720 cores before the A520 cores, starting with the
    highest-performance A720 pair

`conventional` gives you a more standard heterogeneous-core layout.
`performance` puts the A720 cores before the A520 cores.

### IOMMU Capability Description in IORT

IORT is the Arm I/O Remapping Table. Its SMMUv3 entries describe the Arm
System MMU v3 IOMMU blocks. For more background on the SMMUv3 hardware family,
see the Linux `arm,smmu-v3` binding:

- <https://www.kernel.org/doc/Documentation/devicetree/bindings/iommu/arm%2Csmmu-v3.txt>

With `ENABLE_FIRMWARE_FIXES=true`, the firmware advertises HTTU (Hardware
Translation Table Updates) override support in the IORT SMMUv3 entries, so
Linux no longer has to fall back to a more limited default capability view.

### `ramoops` Reserved-Memory Alignment

The ACPI tables already describe a reserved memory range for the `ramoops`
device, but PEI did not reserve that same range in the firmware memory handoff.

With `ENABLE_FIRMWARE_FIXES=true`, PEI also reserves the ACPI-described
`ramoops` range so the reserved-memory story is internally consistent.

### SMBIOS Fixes

With `ENABLE_FIRMWARE_FIXES=true`, SMBIOS reporting becomes more consistent and
reliable for operating systems, inventory tools, and debugging utilities:

- hardware inventory and diagnostic tools no longer see some firmware error
  paths as successful results
- cache totals come from a stable topology model instead of depending on
  whichever CPU happened to boot first
- processor and cache identity come from the Cix/Arm source, while board,
  system, and chassis identity remain identified as Radxa

### DSU PMU Exposure

The DSU PMU is the DynamIQ Shared Unit Performance Monitor Unit: the shared
cluster / L3 performance counter block, mainly useful for diagnostics and
observability.

With `ENABLE_FIRMWARE_FIXES=true`, the firmware exposes the DSU PMU ACPI device
so Linux can discover the shared PMU path that matches the SoC’s cluster / L3
layout.

### Conservative Cache Topology in PPTT

PPTT is the ACPI Processor Properties Topology Table. It describes CPU and
cache topology to the operating system.

Stock firmware exposes CPU hierarchy without a useful cache topology. With
`ENABLE_FIRMWARE_FIXES=true`, the firmware uses a conservative cache model
derived from public platform documentation, SMBIOS, and runtime investigation:

- A520 cores: `32 KiB` L1I + `32 KiB` L1D
- A720 cores: `64 KiB` L1I + `64 KiB` L1D
- A720 cores: private `512 KiB` L2
- shared `12 MiB` L3

The A520 L2 arrangement is still not fully confirmed, so the current model does
not describe an A520 L2 cache.

### eDP Panel Property Cleanup

Stock display ACPI metadata attaches the `edp-panel` property to non-eDP
display paths with empty-string placeholder values.

With `ENABLE_FIRMWARE_FIXES=true`, the firmware only emits the `edp-panel`
property for the real eDP path instead of attaching empty placeholders to DP
or USB-C display outputs that are not panels.

## How to Enable These Fixes

These are `make` variables. Pass them on the `make` command line when you
build.

`ARTEFACT_MODE` selects the build path:

- `ARTEFACT_MODE=custom`
  - enables the local enhancement path
- `ARTEFACT_MODE=upstream`
  - follows the upstream vendor behavior and rejects these fixes

`ENABLE_FIRMWARE_FIXES=true` turns the fixes in this document on.

For example:

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  ENABLE_FIRMWARE_FIXES=true
```

You can also combine that with an optional CPU-numbering mode:

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  ENABLE_FIRMWARE_FIXES=true \
  ENABLE_CORE_ORDER=conventional
```

You can also combine it with the separate experimental UEFI settings overlay:

```bash
make buildbox-firmware-build \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  ENABLE_FIRMWARE_FIXES=true \
  ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true
```

For the broader explanation of those `make` variables and how they interact,
see [`docs/build-variables.md`](docs/build-variables.md).
