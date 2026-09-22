# Qualification review: 22 September 2026

This review uses EDK2 **202608**, with Radxa 1.2.4 and 1.3.1 as the preferred
firmware baselines. Earlier 202605 measurements remain historical evidence;
they are not size qualifications for 202608.

The reviewed build-branch baseline is `defa6833bf`. The original repository's
`main-monorepo-meta` documentation was inspected at `b127942e38`. This is a
reconciliation and audit plan, not a claim that every historical source change
has received a fresh semantic review.

## Debug mask and size preflight

The selected 202608 `MdePkg/Include/Library/DebugLib.h` defines 22 print
categories, whose union is `0x83FB55FF`. Any subset, including zero, is a valid
category mask. `0xFFFFFFFF` is not: its extra `0x7C04AA00` bits are undefined.
This is a category bitset, not a numerical verbosity threshold.

Both the public `make build` entry point and the rendered source validator now
derive accepted bits from the selected source's `DebugLib.h`. Undefined bits
are rejected before rendering/downloading and cannot be overridden. Public
`make help-debug RELEASE=...` prints the exact categories and defaults.

Useful masks include:

| Mask | Categories |
| --- | --- |
| `0x00000000` | None |
| `0x00000001` | Initialization only |
| `0x80000000` | Errors only |
| `0x80000001` | Errors and initialization |
| `0x80000040` | Errors and information |
| `0x83FB55FF` | Every defined 202608 category |

Validity does not establish that the compressed image fits. The 22 categories
give 4,194,304 masks before board, release, features or toolchain variations.
Static string counts or the sum of individual-category sizes cannot prove the
size of an arbitrary combination: calls retain argument computations, compilers
share or eliminate code and strings, and LZMA compression is non-additive.
Even subset/superset relationships are not a strict compressed-size proof.

One failure can be detected without compiling: the inspected 202608 O6 FDF
allocates a `0x400000`-byte FD for full `FIRMWARE_TARGET=DEBUG`, while the
packaging JSON reserves only `0x1F9000` bytes for `bootloader3.img`. The FD
alone exceeds that slot, before the FIP/certificate overhead. Reducing the
print mask cannot fix that structural mismatch. A preflight can reject this
known layout conflict while allowing an explicit compile-only experiment;
changing the flash layout is a separate boot-chain compatibility project.

The implemented preflight rejects known FD/flash-slot conflicts and requires
`FORCE_DEBUG_BUILD=1` for logging-enabled RELEASE experiments. There is currently
no qualified deployable `DEBUG_VERBOSE=true` default. This is an explicit
**unqualified** classification, not a claim that every nonzero mask overflows.
Further verbose builds are deferred while the space issue is unresolved.

The override authorizes compilation only. FV, flash-slot, certificate-chain,
payload-identity and final-output checks remain mandatory. An oversized build
still fails and must not publish an invalid image. Normal compact RELEASE
builds retain their existing logging gates and default mask `0x80000040`.
Verbose RELEASE defaults to all defined categories (`0x83FB55FF` for 202608).

Deterministic LZMA output for fixed bytes is useful for recording measurements;
it does not establish monotonic size across different masks. A failing
`INFO|ERROR` combination does not establish failure of INFO or ERROR separately.
Source, toolchain, configuration and embedded metadata changes invalidate an
exact-size qualification. The final measured size remains authoritative.

### Historical 202608 measurements before the capacity adjustment

Custom 202608/1.3.1 subsequently gained 8 KiB of compressed-FD capacity inside
the unchanged BL33 slot. See the [capacity review](platform-policy-review-20260922.md#release-volume-capacity).
The following measurements retain their original source and `0x1f2000` budget;
they are not verbose qualifications of the updated source.

The public `make build` path was exercised for
`RELEASE=edk2-202608/radxa-1.3.1/unofficial`, O6 RELEASE, custom firmware fixes,
CIX core order, experimental menus enabled, vendor trusted firmware
(`CIX_RELEASE=`), Trixie and the native Arm64 buildbox. GCC was Debian
`14.2.0-19`; `BUILD_DATE` was fixed to `2026-09-22T10:00:00+00:00`.
The rendered source tree was `618b3f458e8e065d31e5c98b9f24ed4103820909`.

| Configuration | Required compressed FV | Result against `0x1f2000` |
| --- | --- | --- |
| Logging-only RELEASE, `DEBUG_VERBOSE=true`, mask `0x80000040` | `0x2031a0` | Rejected: 70,048 bytes too large |
| Logging-only RELEASE, `DEBUG_VERBOSE=true`, mask `0x00000001` | `0x1f3958` | Rejected: 6,488 bytes too large |
| Compact diagnostics, `DEBUG_VERBOSE=false`, mask `0x80000001` | `0x1f1c88` | Packaged successfully: 888 bytes spare |

In particular, initialization-only logging fitting the earlier 202605 build
does not establish that it fits 202608. The INFO build's compiled BDS image
contains its INFO strings; generated compiler inputs retain both `NDEBUG`
definitions and use the private logging header. These are real logging builds.

The compact configuration passed full-flash and OTA packaging, vendor BL1
signature verification and certificate-chain checks. It retains ordinary
RELEASE gating while emitting the explicitly added BDS/setup-migration
diagnostics. Its very small remaining FV space is specific to these exact
inputs; no testing on hardware was performed. The successful 8 MiB full-flash
image's SHA-256 was
`3574e1f504ae879bf395ec72c866a12e83f3be5eae0a212a47ba589303a90bc7`.
The images were qualification outputs, not a published release.

Those three historical builds emitted two AutoGen warnings. The custom repair
removes the erroneous `DpuDxe` library mapping while retaining the driver, and
corrects the selected LZMA INF's class while retaining its constructor. Custom
EDK2 builds now use `build -w`, making AutoGen warnings fatal as well as the
existing compiler warning policy. Upstream source mappings remain unchanged.

See [platform and build-policy review](platform-policy-review-20260922.md) for
current validation results, warning policy and the setup repairs.

## ACPI, signing and BDS status

The integrated fixes represent the identified ACPI Table Upgrade improvements
and include firmware-only memory-reservation and dynamic CPU-mapping fixes.
See [ACPI coverage](acpi-upgrade-coverage.md). This is the established source
comparison, not proof that every device works on every kernel or board.
Subsequent kernel-profile changes should continue to receive a parity review.

Custom builds check BL1 identity/validation, certificate signatures and
delegation, trusted and non-trusted payload hashes, expected vendor anchors,
FIP structure, counters, layout bounds and packaged outputs. Nonblank
`CIX_RELEASE` is rejected. These checks cannot read a board's fused identity
or current rollback state and cannot prove runtime bootability. See
[certificate-chain validation](firmware-chain-validation.md).

CPU-idle migration save order, error handling, retry and variable-size handling
were corrected, and BDS progress diagnostics were added. Their source and
compiled-code checks do not establish the cause of the reported intermittent
hang. Qualification still needs the exact flashed image and uninterrupted UART
capture showing which operation fails to return. Other setup-variable producers,
console/USB/graphics connection and firmware updating remain audit surfaces.

### Fixed-regulator errors

There is no demonstrated firmware change that resolves the reported
`reg-fixed-voltage ... -12` failures. Related GPIO/pin-group ownership defects
were corrected, while regulator devices, polarity and rail policy were retained.
The tester's later report corrects the count to eleven on every variant and
names Ubuntu `6.11.0-29`; an increase caused by Stage 2 is not established.

The current 202608 O6 source exposes `PRP0001` devices with
`compatible = regulator-fixed`, voltage constraints and GPIO references in
`UsbPwr.asl`, `Wireless.asl` and `MipiCamera.asl`. Linux can match a driver using
the ACPI compatible property without that driver supporting ACPI configuration.
The [upstream Linux 6.18 fixed-regulator driver](https://github.com/torvalds/linux/blob/v6.18/drivers/regulator/fixed.c)
still reads Device Tree configuration only when `of_node` exists; otherwise it
expects platform data and returns `-ENOMEM` if absent. A valid `_DSD` property
package cannot populate that C structure by itself. This is not evidence of
physical memory exhaustion, nor of a fix simply arriving in newer Linux.

The currently inspected Gentoo 6.18.53 and 7.2.7 ebuilds explicitly skip CIX's
`0033-regulator-add-acpi-support.patch`. Their comments record unsafe null
handling, reference/allocation handling and coupling/index handling; firmware
retains the rail state. This current policy is not proof of which patches were
in the user's earlier 6.18/7.1/7.2 images. Absence of errors could reflect
different binding, configuration or driver support, rather than correct rail
management.

This issue is **deferred unless it reproduces on the maintained custom kernel**.
No firmware regulator workaround is being added. If reproduced, collect the
exact kernel source/configuration, bound drivers and active ACPI tables before
changing either side of the interface.

## Scope of a renewed audit

The discovered defects justify a structured review of project changes and
uplift conflict resolutions. The useful reason is the failure pattern:
compile success did not prove trust, logging flags did not prove compiled
messages, and rendering did not prove vendor-release fidelity. The model used
to write the earlier code is not a correctness criterion.

At the inspected 202608/1.3.1 checkpoint there are 269 custom entries, including
166 regular files (about 1.97 MB); many remaining entries are intentional
symlinks. The build branch has 133 tracked orchestration/configuration/workflow
files (about 1.44 MB). Excluding the 13 materialized upstream third-party
subtrees, the imported EDK2 component differs from its base in 33 paths.
Platform and non-OSI enablement add a much larger vendor-specific surface.
These counts are scope indicators, not counts of defects or semantic changes.

Recommended passes:

1. Inventory each difference against exact upstream and Radxa inputs. Separate
   materialized submodules, generated files, EOL-only changes and binaries.
2. Review all repository-owned behavior and every active generic EDK2 seam;
   prioritize signing/update paths, allocation bounds, variable migration,
   memory maps, boot/discovery, concurrency and incremental rebuilds.
3. Audit historical conflict decisions and upstream supersession; use patch
   equivalence rather than blindly replaying history or deleting old carries.
4. Prove each advertised release's source ancestry/content. A version string
   or a successful build is insufficient.
5. Run fresh-clone source/render/dependency/AutoGen checks for all combinations;
   fully compile, package and validate representative equivalence classes plus
   every required release/board/fixes combination. Keep upstream replay intact.
6. Add negative and sequential-build tests tied to actual entry points, then
   qualify hardware-sensitive behavior separately.

### Release-combination gap and estimated effort

The current model has 18 EDK2 releases and **360** non-CIX custom unofficial
release targets across 22 Radxa versions. It does not contain every theoretical
18-by-22 pair: 202605 has six Radxa targets and 202608 has two. Do not silently
invent support for the missing pairs.

The required 1.2.4/1.3.1 matrix has 36 release pairs and **144** full-build jobs
after O6/O6N and fixes off/on. Applying that same board/fixes coverage to all
360 currently advertised pairs gives **1,440** builds, before extra distros,
host architectures, debug settings or menus.

The older generic targets still select shared Unofficial source refs and apply
release metadata. For example, `edk2-202502/radxa-1.3.1/unofficial` selects
`source/unofficial/edk2-stable202502` and a `release_metadata` step, without
merging all 1.3.1 vendor source changes. This is a correctness gap, not merely
missing test execution. The release-specific latest checkpoints provide the
stronger baseline. The initial structural inventory accepts 24 baseline/payload
combinations and rejects 336 generic aliases. Public custom builds now fail
early on these mismatches. The catalog and full CI matrix remain intact; those
historical targets need proper integration before the complete matrix can pass.
This initial check is not a complete semantic audit of vendor deltas. Correcting historical fidelity needs explicit source-model
work while preserving required refs and tags.

Planning estimates, assuming one sustained reviewer and no major new defects:

| Work | Engineering effort |
| --- | --- |
| Focused 202608 high-risk source review and inventory | 2–4 working days |
| Thorough review of project changes and retained uplift decisions, without all combination builds | 1–2 working weeks total |
| Add exhaustive advertised-target structural validation and evidence accounting | Another 2–5 working days |
| Repair historical release-specific provenance where missing | Potentially another 1–3 weeks; first inventory should refine this |

Build-machine time is separate: at 5–20 minutes per job, 144 builds cost roughly
12–48 runner-hours; 1,440 cost 120–480 runner-hours, plus setup, retries and
companion checks. Parallelism reduces elapsed time, not total compute. These
are planning ranges, not measured CI duration promises. Debug combinations
need representative and expected-failure coverage rather than millions of
full compilations. Retained TF-A/OP-TEE development/rejection tests remain
useful even though those sources are excluded from flashable combinations.

## Reconciled remaining work

The old metadata branch contains completed tasks and now-invalid assumptions.
Use the classifications below instead of treating every old unchecked item as
new work.

| Priority / state | Item and evidence boundary |
| --- | --- |
| Release qualification | Publish the integrated build changes and matching source refs coherently, then obtain all required CI results at that exact commit. Older green CI is not qualification of the seven newer local build commits. |
| High-value source work | Correct historical Radxa release fidelity and audit all retained uplift/conflict decisions as above. |
| High-value build work | Debug preflight implemented; extend sequential-build checks to interrupted runs, export receipts and copied-file provenance. |
| Implemented warning repair | Corrected both AutoGen library mappings on the custom path; added warning-as-error and constructor-preservation coverage. |
| Reviewed DMA policy | Physical gap and actual UEFI memory map are preserved. Allocation below the scalar bound cannot allocate holes; the high bank remains conservatively excluded from this DMA path. Do not widen the bound without device addressability evidence. |
| Confirmed update-wrapper follow-up | `src/scripts/startup.nsh` prints completion and proceeds to shutdown without checking `FlashUpdate.efi` status. Add custom-only status handling and verify the opaque updater's return/readback contract. |
| Kernel/firmware coordination | Defer regulators unless reproduced on the custom kernel; keep ramoops parser support separate from firmware reservations. Collect the unresolved Wi-Fi `_DSM` function and PCI error details. |
| Reviewed setup policy | Retained the legacy SCMI child-device default; repaired four initializer/read paths, hostile variable sizes and unavailable configuration protocol handling. Other variable consumers remain an audit surface. |
| Experimental menu restored | Restored the existing global SMMU control on the four source refs with its vendor IORT consumer. Independent PCIe-only policy remains a separate feature. |
| Optional source audit | Review Unlocked changes selectively, especially flash verification/error propagation, setup-size handling and PCIe MMIO windows. Recheck actual current source before adopting an old changelog recommendation. |
| Build quality | Complete warning-exception review across retained toolchains and actual builds; improve compact ACPI/FV audit failure summaries; simplify orchestration only where it improves reviewability. |
| Reviewed LF policy | Preserve byte-exact vendor inputs and unchanged mirrors; use LF for repository-owned text. No wholesale normalization is recommended. |
| Optional tooling | Add read-only EC time-series collection, trace comparison for the experimental LaunchFastboot reconstruction, and shipping-HII/open-menu comparison. LaunchFastboot reconstruction does not establish that the separate Fastboot application is open source. |
| Optional packaging | ESP-side opt-in iPXE with explicit Secure Boot validation; keep it outside firmware volumes and do not alter BootOrder automatically. |
| Requires testing on hardware | BDS recurrence; actual updater/version display; firmware menu rebuilding details; O6N, PCIe/SMMU workloads, suspend, PXE, Secure Boot negative tests, CPU numbering, SMBIOS, RTC and experimental settings. |
| Vendor-dependent | Replacing trusted BL31/OP-TEE, full-capacity MTE repair, functional fTPM, and deeper memory/PM settings without proven consumers or authorized payloads. These are not ordinary source-only firmware fixes. |

Already integrated: identified ACPI parity fixes, disjoint audio/ramoops memory
reservations, CPU-idle migration repairs, chain/layout/locked-payload guards,
nonblank-CIX fast failure, the existing System Information page's rebuild
arguments, and logging-only RELEASE semantics. The old proposal to make
custom trusted firmware production-safe merely by substituting operator keys
is invalid without vendor authorization. UEFI Secure Boot PK/KEK/db management
is a separate trust layer.

The recommended order is final 202608 qualification, historical source-fidelity
inventory, high-risk code review, then targeted fixes and matrix expansion.
Feature work should follow those correctness checks.
