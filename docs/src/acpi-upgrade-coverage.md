# Firmware coverage of ACPI Table Upgrades

The firmware fixes must provide everything represented by the maintained kernel
ACPI Table Upgrades. Firmware may provide a better implementation using earlier
initialisation, memory handoff or generated tables. Identical AML is not the
acceptance criterion; equivalent or better behaviour is.

The comparison baselines are the tables shipped with Radxa O6/O6N 1.2.4 and
1.3.1. The kernel profiles are maintained in `gentoo-ebuilds`, under
`sys-kernel/cix-sources/files/acpi-table-upgrade/`. A shipped-table extraction is
not a capture of runtime-generated tables from a booted board.

The primary 202208 and 202608 custom checkpoints provide release-specific
1.2.4 and 1.3.1 baselines. Historical aliases must pass the same source-binding
and vendor-payload guards; a matching version string does not establish that
all named-release changes are present. See [checkpoint maintenance](source-checkpoint-maintenance.md).

## Coverage with `ENABLE_FIRMWARE_FIXES=true`

| Area | Firmware implementation |
| --- | --- |
| PCIe and IORT | Standard PCIe enumeration, five exact ECAM reservations, PCIe SMMU node/root mappings and corrected HTTU flags. |
| GPIO ownership | GPIO descriptors own reset, power and regulator pins; duplicate pin-group claims are removed while clock pins and rail policy are preserved. |
| Shared Type-C interrupt | O6 PD10 and PD11 share GPI4 pin 8. |
| SCMI mailbox | Separate mailbox/shared-memory windows, power-unit attributes and CPU-domain lookup methods. |
| PPTT | The configured fixes PCD selects the cache-ID-capable table revision; it does not depend on an unforwarded compiler macro. Instruction caches do not advertise a valid data write policy; data and unified cache attributes are preserved. |
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
Successful booting and SMMU enumeration do not establish correct behavior across
all PCIe, suspend or GPU workloads.

The ramoops and fixed-regulator warnings can also depend on kernel support for
ACPI firmware-node properties. Correct tables and reserved memory cannot replace
missing driver support. Collect the actual kernel version, full device errors
and live tables before changing rail policy or suppressing devices.

The reported regulator errors have not been reproduced on the maintained
custom kernel. That investigation is deferred; no firmware rail-policy workaround
is being added. Driver matching through ACPI properties does not itself prove
that a driver can parse those properties. Likewise, correct ramoops reservations
do not supply missing kernel configuration support. If reproduced, retain the
exact kernel source/configuration, bound drivers and live tables before changing
firmware.

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
The command above retains ordinary RELEASE compilation. For broader logs,
[logging-only RELEASE](debug.md) can enable selected categories;
[adaptive sizing](debug-layout.md) checks whether a larger BL33 slot is needed.

For another occurrence, retain uninterrupted UART output from reset, the exact
image/build manifest and the previous boot's success/failure history. The last
entry message without its corresponding exit status identifies the next
operation to investigate. No global timeout or automatic reset is added to hide
the failure.
