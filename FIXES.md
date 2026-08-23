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
build path. TF-A source fixes are controlled separately with
`ENABLE_TF_A_FIXES=true` and currently only affect custom `CIX_RELEASE=v1.2`
builds.

## Included Fixes

### Audio DMA Reserved-Memory Alignment

Stock ACPI tables describe several audio and DMA buffers as reserved, but PEI
(Pre-EFI Initialization, the early firmware phase that discovers memory and
hands it to later stages) did not reserve those same ranges.

Stock ACPI also split the audio DMA/HDA area into two `7 MiB` windows that do
not match the vendor Device Tree shared-dma-pool layout.

With `ENABLE_FIRMWARE_FIXES=true`, PEI reserves the DSP, DMA1, and HDA audio
buffers so that the runtime memory map matches the vendor Device Tree
carveouts. The DMA1 reserved-memory lookup entry is also aligned with the
vendor Device Tree layout: `12 MiB` for DMA1 at `0xd0000000`.

The audio DMA350 clock lookup is also corrected so the DMA1 controller consumes
the audio DMAC AXI clock directly instead of pointing its tree-clock lookup at
the unrelated FCH DMA controller.

Custom firmware also publishes the HDA controller's standard ACPI `_DMA`
translation. This describes the full HDA DMA aperture, where device DMA
addresses `0x00000000`-`0x7fffffff` translate to CPU physical
`0x90000000`-`0x10fffffff`. Fixed firmware no longer publishes the HDA `RSVL`
lookup entry, so Linux should use the standard ACPI DMA range instead of a
private coherent DMA pool.

On Orion O6, the HDA sound-card pin group consumes GPIO144 as its `pdb0`
output. Fixed firmware includes that line in GPI3's output-capable mask while
preserving the existing eDP/backlight output lines. O6N does not expose this
sound-card pin group and retains its stock mask.

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

### PCIe Resource Windows and ECAM Reservation

Stock firmware exposes standard `PNP0A08` PCIe root bridges without the I/O BAR
windows that are present in the vendor Device Tree and in the alternate
`CIXH2020` ACPI model. It also exposes an aggregate `PNP0C02` reservation over
the whole ECAM aperture even though each PCI root bridge already reserves its
own ECAM range through the normal MCFG/host-bridge path.

With `ENABLE_FIRMWARE_FIXES=true`, the standard PCIe root bridges gain the
missing `DWordIO` apertures and their adjacent 32-bit memory windows are
tightened so the ranges do not overlap. The aggregate `PNP0C02` ECAM
reservation is hidden on the custom path, leaving the per-root ECAM
description as the authority.

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

### USB-C PD Shared Interrupt Declaration

The two RTS5453 USB-C Power Delivery controllers on Orion O6 share the same
GPIO interrupt line. Stock firmware describes each ACPI `GpioInt` resource as
exclusive, which can prevent Linux from resolving or sharing the Type-C
controller interrupt correctly.

With `ENABLE_FIRMWARE_FIXES=true`, the Orion O6 Type-C PD ACPI entries describe
that GPIO interrupt as shared. Upstream replay builds keep the stock exclusive
declaration.

### SCMI Mailbox Shared-Memory Split

Stock ACPI describes the PM SCMI mailbox devices `MBX6` and `MBX7` as full
`64 KiB` MMIO windows starting at `0x06590000` and `0x065a0000`. It also
describes `SHM0` and `SHM1` as the first `0x80` bytes of those same windows.
Kernels that claim mailbox and SCMI shared-memory resources separately can
therefore see a real MMIO-resource conflict before SCMI clocks, performance
domains, and power domains have a chance to bind.

With `ENABLE_FIRMWARE_FIXES=true`, `MBX6` and `MBX7` start at `0x06590080`
and `0x065a0080`, with their lengths reduced to `0x0ff80`. `SHM0` and `SHM1`
keep the leading `0x80` bytes. This keeps the original mailbox-window end
addresses unchanged while matching the CIX mailbox driver's shared-memory
offset model.

### GPU Cache-Coherency Metadata

Vendor/upstream ACPI marks the `CIXH5000` GPU as cache-coherent. Linux
investigation against the vendor stack showed that Sky1 GPU DMA must instead
be treated as non-coherent.

With `ENABLE_FIRMWARE_FIXES=true`, custom firmware publishes `_CCA = 0` for
`CIXH5000` directly in ACPI. That lets kernels consume the custom firmware
metadata instead of carrying a runtime ACPI scan quirk to override it.

### SCMI Bus Performance Domains

The Sky1 vendor Device Tree describes CI700 and NI700/MMHUB bus performance
controls backed by SCMI DVFS domains, but vendor/upstream ACPI does not expose
equivalent bus-performance devices.

With `ENABLE_FIRMWARE_FIXES=true`, custom firmware publishes ACPI devices for
the CI700 and MMHUB fabric DVFS domains. This lets Linux bind the ACPI-capable
`CIX_BUS_PERF` driver and pin those fabric domains to the highest advertised
OPP during bring-up.

### eDP Backlight Level Table

The Orion O6 vendor Device Tree describes the DP2/eDP backlight brightness
levels as the full integer range `0..255`. Stock ACPI describes the same
backlight device and default brightness, but the ACPI brightness table stops at
`254`.

With `ENABLE_FIRMWARE_FIXES=true`, custom firmware adds the missing final
`255` entry so the ACPI backlight metadata matches the vendor Device Tree.

### USB Device-Model Selector

With `ENABLE_FIRMWARE_FIXES=true`, custom firmware can expose one USB ACPI
model at a time:

- `Linux (PNP0D10)`
- `CIX (CIXH2030/CIXH2031)`

`Linux (PNP0D10)` is the default and is the safer choice for upstream kernels.
It exposes the generic ACPI xHCI-compatible controller objects and hides the
overlapping vendor USB wrappers.

`CIX (CIXH2030/CIXH2031)` exposes the vendor USB wrappers instead. Select it
only when you are deliberately booting a kernel that uses the CIX USB stack.

For O6 and O6N, the default generic USB view is also tightened to the
board-plausible Type-C controller set derived from the public board
documentation and the firmware's own per-board USB policy headers. If
`ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true` is also enabled, the USB setup page
adds controls for the USB model and the default Type-C generic-controller
visibility choices.

### CPU Performance Description Repair (`_CPC` / CPPC)

The ACPI `_CPC` objects are part of CPPC (Collaborative Processor Performance
Control). They describe CPU performance levels to the operating system.

Stock firmware treats SCMI (System Control and Management Interface)
performance levels as if they were already expressed in MHz. With
`ENABLE_FIRMWARE_FIXES=true`, the firmware derives the CPPC frequency scaling
from the SCMI domain attributes instead. It also derives `_CPC`
`ReferencePerformance` from the repaired nominal performance/frequency tuple
and the architectural timer frequency, rather than advertising the stock
fixed value for every CPU.

That keeps the static ACPI CPU-performance view aligned with the same SCMI
performance model the firmware is already using underneath.

### Experimental CPU Thermal Power Model Selector

Stock ACPI and the vendor Device Tree disagree about the CPU thermal zones'
non-standard `SSTP` sustainable-power values. Current Linux ACPI thermal-zone
drivers do not appear to consume these values, but keeping the selector in
firmware lets test builds compare both models without changing the default
ACPI table.

With `ENABLE_FIRMWARE_FIXES=true` and
`ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true`, the experimental UEFI setup menu
adds a `CPU Thermal Power Model` option:

- `Vendor ACPI`
  - preserves the stock ACPI sustainable-power values and remains the default
- `DTB-derived`
  - uses the CPU thermal-zone sustainable-power values observed in the vendor
    Device Tree

This selector only changes the custom-path ACPI `SSTP` values patched at boot.
It does not enable any new thermal policy by itself.

### SoC and EC Thermal Metadata

The vendor Device Tree and platform memory-map headers identify additional SCMI
thermal sensors for VPU, GPU, SoC bridge, DDR, CI700, NPU, SoC trace, and board
NTC monitoring. Stock ACPI exposes only the CPU clusters, GPU-average zone, and
EC board zone, and the EC zone lacks a valid critical trip point.

With `ENABLE_FIRMWARE_FIXES=true`, custom firmware gives the CPU zones clearer
cluster descriptions, adds DTB/MemoryMap-backed monitoring zones for the extra
SCMI thermal sensors, associates GPU thermal zones with the GPU device, and
adds the EC board thermal-zone critical trip point derived from Radxa platform
configuration. These additions are metadata and trip-point repairs; they do not
turn on a new DVFS policy.

### CPU Idle Default Migration (`_LPI`)

ACPI `_LPI` objects describe CPU idle states. Vendor NVRAM can preserve a
disabled CPU LPI setup value even when the custom firmware default enables the
deepest advertised idle state.

With `ENABLE_FIRMWARE_FIXES=true`, firmware performs a one-time migration from
the disabled CPU LPI setting to the custom firmware default. The migration is
recorded in NVRAM, so a later deliberate user change back to disabled is not
overwritten on subsequent boots.

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
not describe an A520 L2 cache. The shared L3 is assigned PPTT Cache ID `1` so
other ACPI tables and Linux cache-topology code can refer to the same physical
cache consistently.

### DSU Cache Allocation Description (MPAM)

With `ENABLE_FIRMWARE_FIXES=true`, firmware publishes an MPAM table for the
live-identified DSU-120 cache-allocation controller at `0x0f010000`. The table
links the controller to PPTT Cache ID `1`, describes its `64 KiB` MMIO window,
and deliberately omits the optional error interrupt until that path has been
qualified.

The table is inert on kernels without Arm MPAM support. On a suitably enabled
Linux 7.1 kernel, it is expected to expose six two-way allocation portions of
the shared `12 MiB` cache through resctrl. The table does not claim cache
monitoring, CI-700 partitioning, device-DMA partitioning, or proportional
bandwidth control. Allocation remains an opt-in feature whose functional
behavior must be validated on target hardware before production use.

### eDP Panel Property Cleanup

Stock display ACPI metadata attaches the `edp-panel` property to non-eDP
display paths with empty-string placeholder values.

With `ENABLE_FIRMWARE_FIXES=true`, the firmware only emits the `edp-panel`
property for the real eDP path instead of attaching empty placeholders to DP
or USB-C display outputs that are not panels.

### Memory Tagging Extension Warning

With `ENABLE_FIRMWARE_FIXES=true`, the SE configuration page makes the MTE
memory-capacity tradeoff explicit in help text and as visible warning text
below the selector.

This does not change the MTE implementation. It warns users that, on systems
with more than `32 GiB` installed, enabling MTE can reduce usable RAM to around
`30 GiB`; on `64 GiB` boards that can remove more than half of installed
memory.

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
