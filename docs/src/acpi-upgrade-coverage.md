# Firmware coverage of ACPI Table Upgrades

The firmware fixes must provide everything represented by the maintained kernel
ACPI Table Upgrades. Firmware may provide a better implementation using earlier
initialisation, memory handoff or generated tables. Identical AML is not the
acceptance criterion; equivalent or better behaviour is.

The comparison baselines are the tables shipped with Radxa O6/O6N 1.2.4 and
1.3.1. The kernel profiles are maintained in `gentoo-ebuilds`, under
`sys-kernel/cix-sources/files/acpi-table-upgrade/`. A shipped-table extraction is
not a capture of runtime-generated tables from a booted board.

The release-specific 202605 and 202608 checkpoints provide the 1.2.4 and 1.3.1
source baselines used for this comparison. Older generic EDK2 targets still
apply release metadata to a shared Unofficial source tree. Porting these ACPI
fixes to those trees does not establish that every older release-labelled
target contains the corresponding vendor release's complete source changes.
That existing source-provenance limitation needs a separate correction; a
successful build or matching version string does not resolve it.

## Coverage with `ENABLE_FIRMWARE_FIXES=true`

| Area | Firmware implementation |
| --- | --- |
| PCIe and IORT | Standard PCIe enumeration, five exact ECAM reservations, PCIe SMMU node/root mappings and corrected HTTU flags. |
| GPIO ownership | GPIO descriptors own reset, power and regulator pins; duplicate pin-group claims are removed while clock pins and rail policy are preserved. |
| Shared Type-C interrupt | O6 PD10 and PD11 share GPI4 pin 8. |
| SCMI mailbox | Separate mailbox/shared-memory windows, power-unit attributes and CPU-domain lookup methods. |
| PPTT | The configured fixes PCD selects the cache-ID-capable table revision; it does not depend on an unforwarded compiler macro. |
| CPU numbering | Runtime SSDT domain names follow actual UIDs, physical cores, disabled cores and the selected core order. Static CIX numbering is not imposed on other modes. |
| Audio DMA | Named AXI clock, DMA1 address translation, no obsolete DMA1/HDA fixed-pool metadata or hard-coded DMA dimensions. |
| DSP and display audio | Channel 8 doorbells alongside channel 9 messages; internal I2S5–I2S9 links do not require debug pins. The 1.3.1 I2S2 pin function remains intact. |
| Graphs | Absolute endpoint path strings and no links from disabled virtual displays. |
| Thermal | O6 EC zone label and 98 °C critical trip; existing zone/trip repairs plus bounded NTC zero-sample rereads and an unavailable result when readings remain invalid. |
| Reboot reason | Read-only registers with the scratch encoding selected from the verified vendor boot-chain reference. |
| GPU coherency | Preserve the native coherent description, matching the maintained kernel replacement tables. |
| Memory ownership | Reserve the full legacy 34 MiB audio range on 1.2, preserve the vendor 50 MiB reservation on 1.3, and split shared runtime allocations around ramoops. This requires firmware memory handoff changes. |

## Intentional differences and qualification

Setup policy remains relevant. The one-time CPU-idle migration enables the
custom default only when no completed migration is recorded. A later deliberate
user disable is preserved. A kernel table replacement cannot reproduce firmware
variable management or earlier power configuration by itself.

The dynamic CPU-domain mapping is preferable to copying a fixed CIX UID list:
firmware knows which physical cores are enabled and which numbering mode was
selected. The tests execute the mapping code for all supported numbering modes
and a disabled-core case.

The PCIe SMMU switch in the inspected source controls table generation. It is
therefore too strong to claim that an initrd IORT replacement cannot expose the
same node. Boot and DMA qualification are still required for that kernel path.
The tester's successful Stage 2 boot and SMMU enumeration do not establish all
PCIe, suspend or GPU workloads.

The ramoops and fixed-regulator warnings can also depend on kernel support for
ACPI firmware-node properties. Correct tables and reserved memory cannot replace
missing driver support. Collect the actual kernel version, full device errors
and live tables before changing rail policy or suppressing devices.

The O6 report describing tests on 20 September 2026 names Ubuntu `6.11.0-29`
and build commit `5fc8d7bb524878d61bf763e69c5988b5956165eb`. It corrects an
earlier comparison: the reporter now counts eleven fixed-regulator errors on
every variant. It also reports that resource-11 conflicts and HTTU override
warnings disappeared with fixes enabled, and the PCIe SMMU node returned.
These observations agree with the inspected source, but the original complete
logs and failing image/readback pair were not supplied. They do not independently
establish build-to-flash provenance or qualify the full board/kernel combination.

Upstream Linux 6.11's [fixed-regulator driver](https://github.com/torvalds/linux/blob/v6.11/drivers/regulator/fixed.c)
returns `-ENOMEM` when a non-Device-Tree device has no platform configuration.
Its [ramoops driver](https://github.com/torvalds/linux/blob/v6.11/fs/pstore/ram.c)
similarly reports `NULL platform data` and `-EINVAL`. Those paths fit the
reported failures; the exact Ubuntu source and backports still require checking.
Firmware reservation fixes do not supply the missing kernel property parser.

The [September 2026 review](qualification-review-20260922.md#fixed-regulator-errors)
also checks Linux 6.18 and the current Gentoo ebuild policy. The former still
expects platform data outside Device Tree; the latter quarantines CIX's unsafe
regulator ACPI patch. A newer kernel version or an absence of errors alone
therefore does not establish working regulator support.

The reported `_DSM` identifier `e8c3a8d2-694b-004f-82bd-fe8607803aa7` matches
the [Realtek rtw89 Wi-Fi interface](https://github.com/torvalds/linux/blob/v6.11/drivers/net/wireless/realtek/rtw89/acpi.c).
Obtain its function number and adjacent driver messages before adding firmware
methods. Radio policy values must not be invented just to remove a warning.

## Intermittent BDS hang

The CPU-idle migration runs in DXE, before the reported BDS banner and ESCAPE
prompt. Its write ordering and variable-size handling needed correction, but
there is no evidence yet that it caused the single observed hang.

Migration writes now log entry/status, commit setup before the completion marker,
and retry failed writes on a later boot. Optional migration failures do not abort
setup initialisation. BDS logs bracket logo setup, device discovery/connection,
boot-option refresh and capsule handling. Enable UEFI diagnostics while retaining
the vendor trusted firmware:

```bash
make build \
  RELEASE=edk2-202608/radxa-1.3.1/unofficial \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  FIRMWARE_TARGET=RELEASE \
  FIRMWARE_DISTRO=trixie \
  ENABLE_FIRMWARE_FIXES=true \
  ENABLE_CORE_ORDER=cix \
  ENABLE_EXPERIMENTAL_UEFI_SETTINGS=false \
  CIX_RELEASE= \
  DEBUG_VERBOSE=false \
  DEBUG_PRINT_ERROR_LEVEL=0x80000001
```

The board's RELEASE compiler flags now respect `DEBUG_VERBOSE`; previously they
could disable logging after the custom include enabled it. Targeted setup/BDS prints also work
with `DEBUG_VERBOSE=false` when the `DEBUG_INIT` mask bit is selected. A build
requesting those diagnostics checks that the BDS markers survived in the EFI.
Global RELEASE logging exceeded the fixed volume even with a narrower mask;
the command above therefore retains ordinary RELEASE compilation.

For another occurrence, retain uninterrupted UART output from reset, the exact
image/build manifest and the previous boot's success/failure history. The last
entry message without its corresponding exit status identifies the next
operation to investigate. No global timeout or automatic reset is added to hide
the failure.
