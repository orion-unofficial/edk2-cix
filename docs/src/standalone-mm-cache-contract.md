# StandaloneMM cache and entry contracts

CIX OP-TEE runs StandaloneMM at secure EL0. Its MM platform must delegate
instruction-cache coherency to the privileged memory-permission service. A
StandaloneMM build that links the ordinary ARM cache-maintenance library can
trap while loading its first driver, even when the FF-A entry interface matches.

## Cache library selection

The CIX/Radxa vendor MM cores link `BaseCacheMaintenanceLibNull`. The recovered
Radxa 1.2.4 and 1.3.1 executables contain that implementation's assertion source
path and its inlined range check. The independent CIX package-tool volume pinned
by ODP at `a3f34cc5fcc8504f6b9028e4678204456462cb3d` contains the same library
source-path evidence. Their executable sections contain none of the
`MRS CTR_EL0`, `DC CVAU` or `IC IVAU` instructions found in the original hybrid
core. This is evidence from the executables, independent of a release label.

The public OP-TEE platform definition,
`Platform/StandaloneMm/PlatformStandaloneMmPkg/PlatformStandaloneMmRpmb.dsc`,
also selects `MdePkg/Library/BaseCacheMaintenanceLibNull`. The generic
`StandaloneMmPkg/StandaloneMmPkg.dsc` selects the ordinary ARM cache library.
A source version alone therefore does not determine secure EL0 compatibility.

For the modern CIX platform, the narrow correction is to replace its AARCH64
`CacheMaintenanceLib` binding with:

```text
CacheMaintenanceLib|MdePkg/Library/BaseCacheMaintenanceLibNull/BaseCacheMaintenanceLibNull.inf
```

This binding is appropriate only with the privileged coherency path below.
Retain `StandaloneMmPeCoffExtraActionLib` and `StandaloneMmMmuLib`. Do not
substitute their null implementations or remove the permission requests.

## Where coherency is performed

The source path for each relocated driver is:

```text
PeCoffLoaderRelocateImage
  -> PeCoffLoaderRelocateImageExtraAction
  -> UpdatePeCoffPermissions
  -> ArmSetMemoryRegionReadOnlyExecPerm
  -> FF-A memory-permission SVC
  -> CIX OP-TEE spm_handle_set_mem_attr
  -> vm_set_prot
```

Driver memory starts writable and non-executable. After loading and relocation,
the extra-action library requests read-only executable mappings for code and
non-executable mappings for data. CIX OP-TEE `core/mm/vm.c:vm_set_prot` remembers
whether the old mapping was writable, updates the translation tables, cleans the
data cache for the affected nonpaged regions, and invalidates the instruction
cache before returning. Its AArch64 helpers execute `DC CVAC; DSB SY` and
`IC IALLUIS; DSB ISH; ISB` at EL1. Initial FD decompression is covered by the
loader's privileged-writable to user-executable permission transition as well.

Older MM code can first remove write permission and then enable execution.
The first transition performs synchronization; the later transition cannot
introduce unsynchronized writes. Keep the legacy extra-action and MMU library
pair together. The CIX loader's nonpaged StandaloneMM allocation is the relevant
case; a paged implementation requires its own pager/coherency review.

The null cache library avoids an extra, unprivileged cache operation after this
privileged synchronization. It is not permission to omit coherency. Verify the
actual linked libraries, successful permission requests, and writable-to-code
transitions whenever changing this platform. A denied execute request must leave
the new driver non-executable. Modeled permission tests do not establish physical
cache behavior or cross-core execution on an O6.

## Exception diagnosis

`ESR_EL1=0x6232c001` identifies `MRS X0, CTR_EL0` (`0xd53b0020`). In the original
hybrid built from component `e3fb003bbb6a6476dddb89b380c518219b9d929b`,
`ArmCacheInfo` is at core RVA `0x7ef8`, FD offset `0x8ef8`. Its caller at RVA
`0x646c` is inlined into `MmDispatcher`; debug information identifies
`ArmDataCacheLineLength`, `InvalidateInstructionCacheRange` and `MmLoadImage`.

At an FD base of `0xc0004000`, these yield fault PC `0xc000cef8` and LR
`0xc000b470`. This mapping requires the matching FD and runtime base. Preserve
the complete UART log, original FD, signed FIP/full-image hash, ELF `.debug`,
linker `.map`, compiler identity and source pins before rebuilding. A matching
reconstruction is strong evidence but does not independently identify a flashed
image whose bytes are unavailable.

CIX TF-A initializes the secure EL1 context from a defined SCTLR value; UCT and
UCI are clear in that path. OP-TEE's read/modify/write setup preserves those
permissions. Record `SCTLR_EL1`, MPIDR and the actual STMM image base immediately
before secure EL0 entry to confirm a particular hardware run. Do not enable
UCT/UCI globally as a replacement for the platform binding correction.

## Independent entry ABI boundary

EDK2 202411 retains the legacy secure-partition boot record. The transition
included upstream commits `6087382c62686130a6cb8bd397afe23bad9c4e67`
(MM CPU entry protocol), `a5212d3db79312f65f0392f294e43193c148199f`
(transfer-list boot), `1c963008e8d7ec8b676478ab42ff40b41cb32be0`
(FF-A 1.2 boot information/HOBs), and
`7340a4b63a993151817faa0adabebfeb007d16c1` (embedded stack).
The entry implementation moved to ArmPkg in
`f0c87b9ef421954ec0e7942d9e8d7d4d14b208b3`. These changes are present in 202502.

Keep the vendor payload on `CFG_STMM_FFA_VERSION=10`. Pair the modern core and
CPU with the maintained mode-12 loader, its ordered MM memory descriptors,
non-secure buffer and CIX NVRAM GUID HOB, and its memory-permission services.
A source-built legacy core additionally needs the CIX appended flash-region
adapter and the corresponding native CPU/FF-A library selections. Fixing the
cache binding does not translate either boot-information format.

## Qualification and integration

The cache-binding candidate has isolated RELEASE builds for both retained
Radxa service sets. Offline checks reproduce the original trap and exercise the
corrected image's initialization, permission changes, variable operations,
authenticated enrollment/deletion, tamper/replay rejection and modeled cold
persistence. A focused test of the actual OP-TEE `vm_set_prot` function checks
cache-call ordering with mocked MMU/cache dependencies.

The maintained StandaloneMM component selects the null cache library when
`CIX_RELEASE=unofficial STMM=unofficial` is requested. `STMM=vendor` remains the
default. The normal renderer supplies the updated component, and its changed
source fingerprint invalidates the StandaloneMM and dependent OP-TEE caches.
Rebuild the complete signed image before hardware qualification. Retain
`STMM=vendor`, release-specific vendor hashes and embedded-payload checks.
The candidate still includes the NOR, serial and firmware-version executables.
Hardware boot, physical cache coherency, signed full-image validation for the
changed FD, real variable persistence and safe FTW recovery tests remain gates.

## First hardware-test build

Use `RELEASE=edk2-202608/radxa-1.3.1/unofficial` for the first test. This matches
the reported failing configuration and retains its modern core/CPU and mode-12
loader, changing the cache binding. The normal-world EDK2 release does not
select the independently maintained StandaloneMM core version. A 202208 target
has separate legacy qualification and selector restrictions; it is not preferable
for isolating this correction. Use Radxa 1.2.4 only when matching a tester's
existing 1.2.4 baseline is the intended comparison.

```sh
make buildbox-firmware-build \
  RELEASE=edk2-202608/radxa-1.3.1/unofficial \
  ARTEFACT_MODE=custom FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE \
  FIRMWARE_DISTRO=trixie ENABLE_FIRMWARE_FIXES=true \
  ENABLE_CORE_ORDER=cix ENABLE_EXPERIMENTAL_UEFI_SETTINGS=false \
  DEBUG_VERBOSE=false CIX_RELEASE=unofficial STMM=unofficial
```

`STMM=unofficial` remains explicitly selected; leaving `CIX_RELEASE` unset
retains the original trusted FIP. Preserve the complete build report and UART
log, then check variable-service initialization and persistence. Successful boot
alone does not establish full variable/FTW or physical cache qualification.
