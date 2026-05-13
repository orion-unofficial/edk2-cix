# Firmware Features

This document lists the firmware-visible fixes and enhancements available in
unofficial firmware builds from `source/unofficial/current`.

The table focuses on changes that affect the firmware image or the behaviour it
exposes to the operating system, setup UI, or shipped payload. The commit
column gives a starting point for deeper investigation.

Except where the table says otherwise, entries require `ARTEFACT_MODE=custom`.
The additional gate column lists any other `make` variables needed.

| Class | Area | Specific change | Additional gate | First commit |
| --- | --- | --- | --- | --- |
| Enhancement | Security: Secure Boot defaults | Use pinned Microsoft-managed UEFI Secure Boot `PK`, `KEK`, `db`, and `dbx` defaults instead of the CIX default key payloads. | None | `e0f006cc0f` |
| Fix | Platform policy: Farm Mode / EC | Omit vendor Farm Mode handling and the related EC farm-command/protocol plumbing. | None | `0566b6918c` |
| Fix | Boot behaviour: Recovery path | Use the standard EDK2 recovery path and normal firmware boot manager instead of a hard-coded Debian `\EFI\DEBIAN\GRUBAA64.EFI` fallback. | None | `fa9df9fc4f` |
| Fix | Firmware metadata: SMBIOS memory map | Report Sky1 SMBIOS Type 19 mapped-address ranges correctly on systems with split low/high DRAM. | None | `ed6fbf3e39` |
| Fix | Firmware metadata: O6 identity | Use O6 Type 1 UUID/serial data from EFI NVRAM when present, otherwise derive it from the fused SoC serial. | `FIRMWARE_BOARD=O6` | `fd2cf4148e` |
| Enhancement | Firmware metadata: O6 memory identity | Report richer O6 SMBIOS Type 17 memory-device data for the validated `PcbSku=4`, `MemType=11`, `BoardRev=0` population, including Samsung / `K3LKCKC0BM-MGCP` identity. | `FIRMWARE_BOARD=O6` | `fd2cf4148e` |
| Fix | Release payload: Metadata hygiene | Ship custom payloads with debug metadata and workspace breadcrumbs stripped from firmware artefacts. | None | `50f0e03c33` |
| Fix | Serial/debug: PL011 UART reliability | Wait for PL011 transmit drain before returning from UART writes, reducing truncated firmware serial output on the custom path. | None | `38d4cc709f` |
| Enhancement | UEFI menu: System information | Relabel the Radxa board field in System Information and decode board revision values as `Rev A` through `Rev D`. | `FIRMWARE_BOARD=O6` | `fd2cf4148e` |
| Enhancement | Early boot: CIX V1.2 inputs | Use the public CIX BIOS V1.2 TF-A and OP-TEE source set with the later public CIX `bootloader1.img` payload. | `CIX_RELEASE=v1.2` | `cfaa4b69a7` |
| Enhancement | Early boot: CIX V1.2 BL2 | Use a source-built CIX V1.2 `bootloader2.img` instead of the Radxa-carried blob for that stage. | `CIX_RELEASE=v1.2` | `1e59985bc6` |
| Enhancement | Serial/debug: Dedicated debug UART | Route firmware `DEBUG()` output to UART3 instead of UART2; this implies UART3 enablement and aligns the pinmux and ACPI debug-port description with that choice. | `DEBUG_ON_UART3=true` | `c7cbf3ec3b` |
| Enhancement | Serial/debug: Debug message mask | Override the firmware `DEBUG()` message mask directly for custom builds. | `DEBUG_PRINT_ERROR_LEVEL=<u32>` | `c7cbf3ec3b` |
| Enhancement | Serial/debug: Verbose release logging | Re-enable firmware `DEBUG()` logging for `RELEASE` builds without switching the whole firmware image to `DEBUG`. | `DEBUG_VERBOSE=true` | `c7cbf3ec3b` |
| Enhancement | UEFI menu: Board power controls | Expose setup controls for RTC wakeup, on-board LAN power, M.2 Wi-Fi / Bluetooth power, and on O6 also graphics-slot and M.2 SSD-slot power. | `ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true` | `078a27ea16` |
| Enhancement | UEFI menu: PCIe SR-IOV | Expose an O6 setup control for SR-IOV handling during PCIe enumeration. | `ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true` + `FIRMWARE_BOARD=O6` | `078a27ea16` |
| Enhancement | UEFI menu: Serial and text console | Expose setup controls for serial-console handoff and UEFI text-console size, including help text for graphics-active behaviour. | `ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true` | `078a27ea16` |
| Fix | ACPI memory reservation: Audio DMA | Reserve the DSP, DMA1, and HDA audio buffers in PEI so the firmware memory map matches the reserved regions already described by ACPI. | `ENABLE_FIRMWARE_FIXES=true` | `0ba4d5e46b` |
| Fix | Hardware support: PCIe ownership | Narrow the PCIe `_OSC` handoff so firmware keeps only SHPC masked and lets the OS own PME, AER, and LTR. | `ENABLE_FIRMWARE_FIXES=true` | `0ba4d5e46b` |
| Fix | Hardware support: PCIe ACPI model | Expose a setup selector for the ACPI PCIe model: standard Linux `PNP0A08` root bridges or vendor-style CIX/Cadence `CIXH2020` devices. | `ENABLE_FIRMWARE_FIXES=true` | `a5d8190553` |
| Fix | Hardware support: SCMI mailbox resources | Split the PM SCMI mailbox windows from the leading `0x80` bytes used as SCMI shared memory, avoiding mailbox/shmem MMIO claim conflicts. | `ENABLE_FIRMWARE_FIXES=true` | `8fc6284656` |
| Fix | CPU performance: CPPC / `_CPC` | Derive ACPI CPPC nominal and lowest frequency values from SCMI domain attributes instead of treating SCMI performance levels as MHz. | `ENABLE_FIRMWARE_FIXES=true` | `343385f2d7` |
| Fix | Hardware support: IOMMU | Advertise HTTU override support in the IORT SMMUv3 entries so Linux sees the intended IOMMU capability model. | `ENABLE_FIRMWARE_FIXES=true` | `0ba4d5e46b` |
| Fix | ACPI memory reservation: `ramoops` | Reserve the ACPI-described `ramoops` range in PEI so persistent-crash-log memory is not simultaneously described as general RAM. | `ENABLE_FIRMWARE_FIXES=true` | `343385f2d7` |
| Fix | Firmware metadata: SMBIOS ownership | Repair SMBIOS OEM hook failure handling, stabilize cache totals, and keep CPU/cache identity owned by the common CIX/Arm path while Radxa keeps board/system/chassis identity. | `ENABLE_FIRMWARE_FIXES=true` | `343385f2d7` |
| Fix | Hardware observability: DSU PMU | Expose the DSU PMU ACPI device so Linux can discover the shared cluster / L3 performance-counter block. | `ENABLE_FIRMWARE_FIXES=true` | `0ba4d5e46b` |
| Fix | CPU topology: PPTT cache model | Describe a conservative PPTT cache topology: A520 L1I/L1D, A720 L1I/L1D plus private L2, and shared 12 MiB L3. | `ENABLE_FIRMWARE_FIXES=true` | `0ba4d5e46b` |
| Fix | Display ACPI metadata | Emit the `edp-panel` property only for the actual eDP path instead of attaching empty-string placeholders to non-eDP outputs. | `ENABLE_FIRMWARE_FIXES=true` | `0ba4d5e46b` |
| Enhancement | CPU topology: Exposed CPU ordering | Remap the CPU UIDs exposed to the OS to either little-cores-first (`conventional`) or performance-first (`performance`) order while keeping physical-core policy attached to the same hardware cores. | `ENABLE_FIRMWARE_FIXES=true` + `ENABLE_CORE_ORDER=conventional` or `performance` | `343385f2d7` |
| Fix | Firmware image layout: DEBUG BL33 | Enlarge the Radxa BL33 firmware volume for O6/O6N `DEBUG` builds so debug images have enough space. | `FIRMWARE_TARGET=DEBUG`; valid with `ARTEFACT_MODE=custom` or `upstream` | `bd8d037cd8` |
| Enhancement | Firmware metadata: O6 asset tags | Set O6 SMBIOS Type 2 and Type 3 asset-tag strings together or independently at build time. | `FIRMWARE_BOARD=O6` + `O6_SMBIOS_ASSET_TAG=<text>`, `O6_SMBIOS_BASEBOARD_ASSET_TAG=<text>`, or `O6_SMBIOS_CHASSIS_ASSET_TAG=<text>` | `fd2cf4148e` |
| Enhancement | Serial/debug: UART3 OS exposure | Mux the UART3 header for UART use and expose it to the OS through firmware metadata without moving firmware `DEBUG()` output there. | `UART3_ENABLE=true` | `c7cbf3ec3b` |
