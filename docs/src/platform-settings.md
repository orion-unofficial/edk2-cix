# Platform settings and limits

These notes describe the maintained custom firmware, particularly the
Radxa 1.3.1 O6 source. Availability depends on the selected board and release.
Experimental menus expose existing platform controls; their presence is not
qualification of every value on every board.

## SMMU and SCMI

Where the vendor source has `ConfigureIortForSmmu`, the experimental menu retains
its global SMMU control. It selects SMMU-bearing versus vendor no-SMMU IORT
contents; PCIe participates when firmware fixes enable its SMMU node. This is
not an independent PCIe-only toggle. Older sources without that consumer do not
receive a nonfunctional control.

`EnableAcpiScmi=0` remains the default. The SCMI parent transport is already
exposed in the inspected ASL; this switch gates legacy CIX DVFS/clock child
devices (`CIXHA008`/`CIXHA009`). Deliberately saved settings are preserved. Do not
interpret the switch as disabling all standard SCMI communication.

## O6 memory speed

The Radxa 1.3.1 source enables memory-configuration updates. The experimental
menu includes Auto, 6000 and 6400 MT/s, encoded as a sentinel, 3000 and 3200 MHz.
The vendor update driver is binary-only. A menu choice and a saved variable do
not prove that the installed DRAM, board profile or training firmware accepts
the requested speed.

Auto is `0xffff`: leave the frequency override unset and let the board/DRAM IDs
select a profile. The 1.3.1 O6 source contains these upper profile rates:

| Profile | Upper transfer rate |
| --- | ---: |
| Ordinary 8/12/16/32 GiB, plus 32 GiB Hynix | 6000 MT/s |
| 24 GiB HS, 32 GiB x8 HS, 48 GiB HS, 64 GiB x8/Rayson | 5500 MT/s |
| 16 GiB HS and 32 GiB Low | 4800 MT/s |

The source definitions are in O6 `mem_config/RS600/GlobalConfig.c` and
`default/{BoardIdMap,MemBiosSetup,QuickConfig}.c`. They establish profile ceilings,
not the instantaneous clock. DDR training precedes UEFI, and vendor DVFS can
change frequency later. There is no established rule that the selected speed
only takes effect at ExitBootServices.

For a particular board, collect its board/DRAM IDs, cold-boot training log and
actual DDR/DVFS measurements during UEFI and Linux. SMBIOS maximum/configured
speed fields alone are not clock measurements. The
[O6 manual](https://dl.radxa.com/orion/o6/docs/rad-doc-0123_radxa_orion_o6_user_manual__version_0.8_g148a184.pdf)
describes operation up to 5500 MT/s; a universal 6500 MT/s chip rating has not
been established. O6N part specifications must not be substituted for O6.

## DMA and the physical memory gap

The vendor map starts low DRAM at `0x80000000` and places capacity beyond the
30 GiB low bank at `0x8000000000`. The physical gap is not RAM and remains absent
from the UEFI memory descriptors.

`UpdatePcdDmaDeviceLimit` computes a scalar low-base-plus-capacity ceiling that
can fall inside that gap. Its `NonCoherentDmaLib` consumer uses
`AllocateMaxAddress` over actual descriptors, so it cannot allocate the hole.
The current policy conservatively excludes the high bank from this DMA path.
Widening it requires evidence of device address widths and translation paths;
it must not invent contiguous memory across the gap.

## Setup state and boot diagnostics

Custom setup initialization checks variable sizes and read errors before using
or overwriting stored data. Recognized older layouts preserve their saved
prefixes; unknown larger schemas and device errors do not silently overwrite
settings. The CPU-idle migration commits setup before its completion marker and
retries failed writes on a later boot.

Those repairs do not establish the cause of the reported intermittent BDS hang.
The [ACPI and BDS guide](acpi-upgrade-coverage.md#intermittent-bds-hang) describes
which logs and image identities to retain if it recurs. Full image updates,
variable-store changes and Secure Boot enrollment are separate operations.
