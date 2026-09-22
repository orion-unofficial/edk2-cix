# Platform and build-policy review: 22 September 2026

Baseline: EDK2 202608, Radxa 1.3.1, custom O6 firmware. This review distinguishes
source checks, actual builds and behavior still requiring testing on hardware.
The original repository's legacy metadata branch was checked at `b127942e38`.

## Warnings and verbosity

Both artifact modes should resolve warnings and errors. Upstream replay must
also preserve vendor artifacts byte for byte. Where a known vendor warning
cannot be fixed without changing those artifacts, only that identified warning
may be suppressed with `V=0 DEBUG=0`. Unknown warnings and all failures must
remain visible. Neither variable suppresses a command's failure status.

- `V=1`: detailed build output, including raw EDK2 commands and diagnostics.
- `DEBUG=1`: tooling tracebacks and unfiltered EDK2 diagnostics. It does not
  select debug firmware or change firmware logging.
- `FIRMWARE_TARGET=DEBUG`: firmware build target, with different code gates and
  FD layout.
- `DEBUG_VERBOSE=true`: custom RELEASE logging, retaining RELEASE assertions
  and other code gates; currently requires `FORCE_DEBUG_BUILD=1`.

The old upstream filter suppressed arbitrary warnings and entire IASL warning
blocks. That was broader than the intended policy. It now recognizes specific
known messages; new warnings remain visible. Existing documented exceptions
include serial LTO, the vendor RWX-segment warning, VFR ambiguity/default
messages, and the two exact AutoGen library mismatches. A changed class/module
or a linker error is not covered by those AutoGen/RWX exceptions. IASL warnings
without an exact reviewed exception remain visible.

The metadata branch's `build-reproducibility.md` records that attempted IASL
cleanups changed AML hashes. Its vendor carry-forward notes preserve imported
bytes. These support targeted exceptions, not blanket suppression or hiding
errors. Custom builds get source repairs instead: remove the invalid DpuDxe
library-class binding and correct the LZMA library declaration without removing
its constructor. Custom EDK2 AutoGen warnings are now fatal (`build -w`).

The source-level changes execute inside the delegated firmware build and/or
compiled firmware. They therefore require retained-source propagation. The
public validators, regression tests, help and documentation remain exclusively
on the build branch.

## RELEASE volume capacity

Custom 202608/Radxa 1.3.1 O6 and O6N now use a `0x1f4000` compressed FD budget,
8 KiB above the imported `0x1f2000`. This consumes existing BL33-slot headroom;
it does not move a flash boundary. The full-flash/OTA slot remains `0x1f9000`.
The measured vendor-delegated FIP overhead was `0x12d3`, leaving `0x3d2d` bytes
of slot slack with the larger FD. Actual package-size checks remain mandatory.

PrePi and the memory map derive the loaded FD extent from `PcdFdSize`. The
platform reserves `[0x84400000, 0x84800000)` for the FD, so the extra two 4 KiB
blocks remain inside that existing RAM reservation too. Upstream FDFs and both
flash JSON layouts are unchanged. Full DEBUG's 4 MiB FD still cannot fit the
flash slot. Other firmware baselines retain their existing volume budgets.

This is room for the normal safety checks and experimental menu, not a claim
that verbose RELEASE masks now fit. Preflight reads the effective custom FDF,
including experimental-overlay precedence and mirrors. The experimental
202608/1.3.1 FDF now mirrors the normal custom FDF instead of carrying a
second capacity definition. Further verbose qualification remains deferred. The final
firmware still needs testing on hardware, including update and boot behavior.

## Structural release identity

The initial guard checks the selected Unofficial checkpoint's recorded Radxa
baseline and exact vendor boot/runtime payload identities against the requested
release. Generic compatibility refs without a release-specific binding use
their actual baseline. It rejects metadata-only relabeling. Optional payloads
absent from both vendor and selected source are permitted; missing or changed
payloads are rejected. Later packaging independently validates the actual
assembled image, its certificate chain and locked payloads.

The inventory contains 360 non-CIX targets: 24 pass these initial checks and
336 generic historical aliases fail. The passing groups are Radxa 1.2.1 on
202208 through 202602, all six explicit 202605 checkpoints, and both 202608
checkpoints (1.2.4 and 1.3.1). For example, the generic 202208/1.3.1 alias still
selects a 1.2.1 baseline and wrong BL1/BL2 payloads. It now fails before a build
can advertise that image as 1.3.1.

This is a first structural guard, not proof that every vendor code delta is
correctly represented. Checkpoint bindings, component-delta accounting and
uplift resolutions still need the broader semantic audit. The supported-target
catalog and full qualification matrix have not been pruned to hide this gap.
A complete green matrix requires correcting the historical source targets.

The 144-job latest-two-release matrix and 1,440-job full catalog estimate
already excluded CIX TF-A/OP-TEE choices and verbose logging. Disabling those
choices therefore does not reduce these particular counts.

## DMA address gap

The vendor memory map places low DRAM at `0x80000000` and relocates memory above
30 GiB of low-bank capacity to `0x8000000000`. `ReportDramHighSpace` reports the
high bank separately. The physical gap must remain absent from the RAM map.

`UpdatePcdDmaDeviceLimit` computes a scalar low-base-plus-capacity ceiling, which
can land inside the gap. Its `NonCoherentDmaLib` consumer allocates using UEFI
`AllocateMaxAddress` over actual memory descriptors and bounces buffers beyond
the ceiling. The scalar value does not make the hole allocatable. Current
behavior conservatively excludes the high bank from these DMA allocations.

No DMA code change is justified solely by this formula. Widening the limit to
the highest bank would need proof of each affected device's address width and
translation path. The address-map hole is preserved; its physical presence is
consistent with the vendor architectural map, not a reason to invent contiguous
RAM. This review does not claim an independent silicon specification audit.

## SCMI and setup-variable review

`EnableAcpiScmi=0` remains the default. In the inspected ASL, the SCMI parent is
already exposed; the switch gates the legacy CIX DVFS/clock child devices
(`CIXHA008`/`CIXHA009`). It does not simply turn all standard SCMI transport off.
The existing help records compatibility problems on older Radxa kernels.
Changing this default without a demonstrated consumer requirement is unwarranted.
Deliberate saved settings remain preserved.

The review did find defects, repaired on the custom path:

- Network initialization could clear a stack structure using an oversized
  length returned by `GetVariable` after `EFI_BUFFER_TOO_SMALL`.
- System-table initialization could write beyond its fixed-size structure using
  that returned length.
- Platform and Radxa initializers could overwrite saved data after unrelated
  read failures. Unknown larger schemas and device errors now fail without
  overwriting variables; recognized short schemas preserve saved prefixes.
- ReadyToBoot ACPI handling could use uninitialized setup data or dereference
  an unavailable configuration protocol. It now initializes fallback values
  and checks the protocol and its data before updating fan AML.

The same module's CPU-share and configuration consumers now share an exact-size
read helper, rejecting truncated successful reads instead of consuming an
uninitialized tail. System-table reads also check their size. Allocation failure
and failed publication of the setup-info protocol return an error rather than
dereferencing NULL or announcing setup initialization complete.

Host C regression harnesses exercise real initializer bodies, including hostile
lengths, read errors, absent variables and supported old layouts. Older retained
RTC-reset policy is preserved. This bounded review does not certify every
variable consumer in the firmware, or establish the cause of the BDS hang.

## Experimental SMMU and memory menus

The vendor global SMMU control existed but was omitted from our experimental
menu copy. It is restored in the four retained refs with the actual
`ConfigureIortForSmmu` consumer: 1.3.0/202605, 1.3.1/202605, 1.3.1/202608 and
1.3/current. It selects the SMMU-bearing versus vendor no-SMMU IORT and includes
PCIe when firmware fixes enable the PCIe SMMU node. This is a global control;
an independent PCIe-only toggle needs a separate table-generation feature.
No dead menu is added to older source lines without that consumer.

Radxa 1.3.1 enables `MEM_CONFIG_UPDATE_SUPPORT`. The experimental menu retains
6000 and 6400 MT/s (stored as 3000 and 3200 MHz), plus Auto. The configuration
parameter mapping and the MemConfigUpdateDxe binary's variable-save callback
are present. The update driver is a vendor binary, however; source inspection
alone does not prove what every profile accepts or the live clock rate.

DDR training precedes UEFI. The expected path is saved configuration for a
subsequent boot, with vendor DVFS also involved. There is no evidence that a
frequency change specifically waits for ExitBootServices. To establish actual
behavior, record training output and actual clock/DVFS state during UEFI and
Linux with identical settings. SMBIOS maximum/configured-speed fields alone
are insufficient clock measurements.

A universal 6500 MT/s chip rating is unconfirmed. The [O6 manual](https://dl.radxa.com/orion/o6/docs/rad-doc-0123_radxa_orion_o6_user_manual__version_0.8_g148a184.pdf)
describes operation up to 5500 MT/s; that is not a part's speed-bin specification.
[Radxa's O6N discussion](https://forum.radxa.com/t/orion-o6n-shipping-delay-notice-community-q-a/29804)
identifies a switch to Hynix parts rated 6400 MT/s and tested at 6000 MT/s on
that revision. It does not establish a 6500 rating for all O6 boards. No 6500
option is added without exact part/board identification and a supported vendor
training/rate encoding. Manufacturer identity alone would be an inadequate gate.

## Proposed startup.nsh change

The vendor script runs `FlashUpdate.efi`, then prints success and shuts down
without checking its exit status. Proposed custom-only behavior is to capture
and check `%lasterror%` immediately, stop on failure, and avoid a false success
message or automatic shutdown after an error. The upstream script must stay
byte-identical. A host-side receipt should name the selected image and hash.

This proposal is not implemented in this change: the opaque updater's actual
exit-status/readback contract still needs verification. A zero exit status
cannot prove that the intended image was fully written. The wrapper must not
promise readback verification that the updater does not provide.

## Regulators and line endings

Regulator investigation is deferred unless the issue reproduces on the
maintained custom kernel. No firmware workaround for the suspect vendor kernel
patch is being added.

There is no blanket LF-normalization migration. Imported files can retain CRLF;
customized overlay files commonly use LF, while unchanged files are mirrored by
symlinks. It is not an enforced rule that every edited file is automatically
converted. The old metadata branch explicitly left wholesale normalization as
a separate policy decision requiring reproducibility checks.

Recommendation: retain exact vendor/base bytes and unchanged mirrors; use LF
for repository-owned scripts, docs and new text. Avoid incidental whole-file
EOL churn in upstream-derived files. Use normalized comparison views for audits,
not indiscriminate conversion of imported trees. OpenSSL and other dependencies
contain byte-sensitive test fixtures and generated data; an automatic text
heuristic does not establish that conversion is safe. A future normalization
layer would need an explicit file allowlist, separate commits and byte-identity
qualification. No global conversion is needed for the current fixes.
