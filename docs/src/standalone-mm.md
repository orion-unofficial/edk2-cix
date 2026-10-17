# CIX StandaloneMM

`ARTEFACT_MODE=custom CIX_RELEASE=v1.2` builds BL31 and OP-TEE from the curated
CIX sources and embeds a pinned vendor StandaloneMM image in OP-TEE. The
StandaloneMM image provides the secure SPI-NOR, UEFI variable and
fault-tolerant-write services required by the existing UEFI firmware.

Leaving `CIX_RELEASE` unset retains the selected vendor trusted firmware.
The standalone image is an embedded component, not a separately flashable
firmware update. Current O6 deployment uses the complete 8 MiB flash image
from the UEFI shell; the generated OTA image is not currently usable.

## Trusted firmware selectors

| Selection | OP-TEE and StandaloneMM |
| --- | --- |
| `CIX_RELEASE=` | Stock vendor `bootloader2.img` |
| `CIX_RELEASE=1.2 STMM=vendor` | Existing curated CIX source and pinned vendor StandaloneMM; `v1.2` is an alias |
| `CIX_RELEASE=unofficial STMM=vendor` | Maintained OP-TEE component and pinned vendor StandaloneMM |
| `CIX_RELEASE=unofficial STMM=unofficial` | Hybrid payload built from EDK2 202608 sources plus three pinned vendor executables; Integrated Radxa 1.2.4/1.3.1 O6 targets with EDK2 202502 or newer |

`STMM=vendor` is the default. Source-built trusted firmware remains restricted
to custom O6 builds with the reviewed signing and pinned-payload integration.
`CIX_RELEASE=unofficial` is available on the active 1.3 line and the Radxa 1.2.4/1.3.1
checkpoints for EDK2 202208, 202605 and 202608. Other historical targets must
first receive and qualify those prerequisites; accepting a selector alone
does not make them supported.

The maintained component is
`source/unofficial/cix/1.2/op-tee/3.17.0`, descended directly from the unchanged
public `source/vendor/cix/1.2/op-tee` release. It preserves the existing curated
compiler adaptations, completes the applicable ODP PR 85 changes, returns the
CIX key-retrieval status (issue 87), and fails closed when the CIX entropy
adapter receives a failed or incomplete hardware read (issue 86).
The renderer selects this component into `src/cix-v1.2/tee-unofficial` using
`config/refs-cix.json`; the existing `tee` subtree remains the `1.2` selection.
Future component changes advance that one branch and its build metadata.

The source-tree selectors execute inside the rendered Makefiles so direct
worktree builds enforce the same restrictions as the outer build command.
Use a materialised source target for `CIX_RELEASE=unofficial`; the component
is assembled by the renderer, not copied into every retained source branch.

## Payload selection and verification

The selected stock `bootloader2.img` hash determines the StandaloneMM checkpoint.
The build accepts only the recorded Radxa 1.2.4 and 1.3.1 inputs. It does not
select by a filename, version string, latest release or host directory contents.

| Radxa checkpoint | StandaloneMM FD SHA-256 | Compressed bytes |
| --- | --- | ---: |
| 1.2.4 | `813a4b29954c11a0ad8a151b81f67852eccd03e7273e26ad7ecf25abad2f60dd` | 306964 |
| 1.3.1 | `bcf541629e956efe8e8f06738314b5b43fcca32340ab3afc62d8723f39e380a1` | 306907 |

Both decoded firmware volumes are 2 MiB. The compressed files are exact
payloads recovered from the corresponding vendor OP-TEE binaries. Their
stock FIP, OP-TEE, compressed-stream and decoded-image hashes and source
provenance are recorded in the rendered tree under
`src/cix-v1.2/release-payloads/standalone-mm/manifest.json`.

Primary qualification targets are EDK2 202208, the CIX fork baseline, and
the current latest EDK2 release (202608). Interstitial releases, including
202605, receive ordinary historical-checkpoint treatment.

Before calculating trusted-firmware cache keys, the Make rule validates the
selected pin and stages `Firmwares/BL32_AP_EFI_STMM.fd`. Repeated staging
preserves its timestamp when the bytes are unchanged. A modified staging
copy is restored from the verified pin. Missing or corrupted pinned inputs,
unknown vendor FIPs and invalid compressed streams fail the build.

The trusted-firmware helper also stages the pin when invoked directly and
always supplies `CFG_STMM_PATH` to OP-TEE. Before signing, it verifies that
the freshly built or cached OP-TEE contains exactly one copy of the expected
standalone image. `Firmwares/vendor-stmm.json` records its identity and binds
the verification to the OP-TEE payload hash. The trusted-component qualification
command checks that this hash matches the payload in the signed FIP.

## EDK2 release and boot-interface boundaries

The EDK2 version used for the normal-world UEFI payload in `bootloader3.img`
does not select the StandaloneMM version embedded in `bootloader2.img`.
The latter contains TF-A BL31 and OP-TEE BL32; OP-TEE loads its embedded
StandaloneMM payload. A 202608 UEFI build can therefore use the matched
legacy OP-TEE/StandaloneMM pair without adopting a newer MM boot interface.

Upstream EDK2 202411 is the last retained stable release whose ARM
StandaloneMM core entry code consumes `EFI_SECURE_PARTITION_BOOT_INFO`.
Starting with 202502, the entry implementation in
`ArmPkg/Library/ArmStandaloneMmCoreEntryPoint` consumes FF-A boot information
with HOBs or a firmware handoff transfer list. CIX OP-TEE v1.2 instead passes
`stmm_boot_info`, CPU information and appended CIX flash-region information.
The incompatibility concerns this OP-TEE-to-StandaloneMM handoff; it is not a
202608 TF-A requirement or a reason to reject newer normal-world UEFI builds.
The last legacy entry implementation is not, by itself, qualification of a
generic 202411 image for CIX NOR storage and platform services.

`make CIX_RELEASE=v1.2` is protected by exact payload selection: the build
stages the approved vendor image and rejects an OP-TEE output that does not
embed it before signing. A valid firmware-volume header or an EDK2 version
at or below 202411 is insufficient approval for a replacement. Keep this
check when adding source or hybrid StandaloneMM builds; qualify their own
entry/CPU interfaces and payload identity independently of the UEFI release.
Do not introduce a blanket EDK2 release ceiling for `bootloader3.img`.

The pinned-payload integration executes inside the rendered source tree because
direct developer builds do not invoke the outer build-branch caller. The helper
and payload inputs therefore belong to the retained firmware source;
qualification and report orchestration remain on `build`.

## Modern StandaloneMM source integration

The modern boot interface is implemented on the maintained component branch,
behind `CFG_STMM_FFA_VERSION=12`. The default, `10`, retains the vendor-image
interface. The component rejects the two known vendor payloads when selected
with the modern interface. Upstream
OP-TEE supplies an FF-A 1.2 StandaloneMM loader interface and EFI HOB builders.
The backport retains the CIX NOR and debug services and leaves the global
OP-TEE FF-A version unchanged. Its HOB allocator corrects the upstream
free-memory-top calculation and checks bounds and arithmetic overflow.
Both configurations compile in the Linux buildbox, including the modern loader
with the hybrid payload described below. Focused C tests exercise the actual
helpers with mocked hardware. Failed TF-A NOR transactions terminate the secure
partition rather than returning to the vendor driver, which ignores failed
direct-response status. Subsequent recovery requires reopening the failed
partition; this does not make an interrupted flash update atomic.

The loader must provide an FF-A boot-information header and HOB descriptor,
the PHIT and firmware-volume HOBs, the non-secure communication-buffer HOB,
and the MM memory-reservation HOB. Modern ARM entry code expects the latter's
first four descriptors to describe the image, secure shared buffer, normal
communication buffer and heap, in that order. The CIX loader already knows
these addresses and sizes. Describe the decompressed image allocation, not
the compressed input length, and validate all bounds before entering it.

The runtime adaptation must also implement version/endpoint queries,
`FFA_MEM_PERM_GET`, `FFA_MEM_PERM_SET` and initialization completion through
`FFA_MSG_WAIT`. Modern entry assembly requests permissions for its embedded
stack before entering C code. Build the modern MM core and CPU driver together:
their entry-point protocol differs from the legacy CPU-entry HOB contract.
The existing direct-request MM communication path remains usable.

Preserve the CIX NVRAM GUID HOB consumed by the NOR, variable and FTW drivers.
Its GUID is `4c8167fc-5a9d-468e-a942-861fb50ab539`. Its payload is 36 bytes:
three little-endian 64-bit bases (variable, FTW working, FTW spare), followed
by three little-endian 32-bit sizes in the same order. This layout is recovered
from both supported vendor core binaries and matches the CIX OP-TEE flash
information. Serialize these fields explicitly; a naturally aligned C
structure may include trailing padding. The containing HOB is aligned to
eight bytes. Retaining the vendor NOR driver still requires this HOB.

These contracts establish a source-based integration route, not a qualified
replacement image. The public upstream RPMB example is not the CIX NOR target.
Retained CIX service executables need their MM protocols, buffer conventions,
PCD configuration and relocations checked against the replacement core. Keep
the pinned production payload until a replacement passes the qualification
below; do not weaken the payload guard merely to admit a newly built FD.

## Hybrid qualification target

`source/unofficial/stmm/edk2-stable202608` contains a reproducible qualification
platform, pinned in `config/refs-cix.json`. It descends from the upstream EDK2
202608 component, materializes the required dependencies, and backports the
upstream CryptoPkg native-width integer fix. It is consumed by the qualification
build command and by firmware rendering at `src/cix-v1.2/stmm-unofficial`.
`STMM=unofficial` builds this component independently of the normal-world EDK2
selection, using the maintained OP-TEE loader in FF-A 1.2 mode.

| Component | Implementation |
| --- | --- |
| StandaloneMmCore | EDK2 202608 source |
| StandaloneMmCpu | Matching EDK2 202608 source |
| VariableStandaloneMm | EDK2 source, authenticated variables and variable policy |
| FaultTolerantWriteStandaloneMm | EDK2 source |
| NorFlashStandaloneMm | Pinned release-specific Radxa executable, 36 KiB |
| SerialOutStandaloneMm | Pinned release-specific Radxa executable, 16 KiB |
| FwVersionStandaloneMm | Pinned release-specific Radxa executable, 20 KiB |

The three remaining secure-side implementations are absent from the inspected
public CIX sources. Their exact executable and dependency-section hashes are
checked during the build. The selected vendor FIP determines the release, and
the recovered service manifest must match that release's pinned vendor FD hash.
The 1.2.4 and 1.3.1 executables differ and are retained separately. The CIX OP-TEE bridge and TF-A xSPI transport remain
source-built; a replacement must preserve their flash-controller ownership.

The platform binds the variable and FTW PCDs through the standard FVB
`GetPhysicalAddress` method and the CIX NVRAM HOB, without relying on private
vendor structure offsets. Its FVB dependency must be satisfied before the PCD
constructor runs. The variable driver also requires
`VarCheckPolicyLibStandaloneMm`: linking the policy engine without its
constructor leaves policy disabled and permits authenticated deletion without
signature verification.

To build the isolated qualification FD:

```sh
docker build -f scripts/stmm.Dockerfile -t edk2-cix-stmm-qualification .
python3 scripts/build_stmm_qualification.py \
    --output .worktrees/stmm-qualification
```

Add `--radxa-release 1.2.4` to select the recovered 1.2.4 services; the default
is 1.3.1. The output directory must be new. It retains the exact source export, build log,
compiler and image identities, vendor checksums, `BL32_AP_MM.fd`, and `build.json`.
The report deliberately says `built-not-qualified`. The FD requires
`CFG_STMM_FFA_VERSION=12`; it is not a complete or independently flashable board
image. Repeat into a second directory to compare clean-build payload hashes.

### Current qualification boundary

Executing the actual hybrid AArch64 payload with the C-generated OP-TEE handoff
and modeled NOR services exercises initialization, memory permissions, ordinary
variable operations, authenticated PK enrollment/deletion, replay/tamper
rejection, and cold-start persistence. These checks are useful offline evidence;
they do not establish hardware boot, flash timing or power-loss behavior.

The MVP acceptance criterion is no observed regressions against the recovered
vendor components under the same qualification conditions. Improving an inherited
failure is deferred work, rather than an automatic prerequisite for an MVP.
The paired 189-point reclaim-interruption sweep has identical outcomes: both
implementations recover in 149 cases and lose the variable in the same 40 cases,
with matching return statuses and recovered values. Comparison with recovered
components does not establish equivalence to every stage of the complete vendor
boot chain.

### Deferred NOR recovery improvement

A modeled interruption after erasing the variable FV header causes the retained
NOR executable to reformat the store on its next initialization, before FTW can
recover from the spare area. The recovered vendor variable and FTW executables
reproduce the loss. Replacing those two components with upstream sources alone
therefore cannot fix the NOR initialization/recovery ordering.

Investigate recovery before any destructive format decision, validating the FTW
metadata and spare contents. Also establish whether another stage of the stock
vendor boot chain performs earlier recovery. Returning a flash error or changing
only the FF-A handoff does not address loss of power between successful flash
commands. Keep the vendor comparison and interrupted-flash fixtures when changing
this behavior. A newly introduced failure remains an MVP blocker.

### Building a hardware-test candidate

From the build branch, select the latest maintained firmware sources explicitly:

```bash
make buildbox-firmware-build \
    ARTEFACT_MODE=custom FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE \
    FIRMWARE_DISTRO=trixie ENABLE_FIRMWARE_FIXES=true \
    ENABLE_CORE_ORDER=cix ENABLE_EXPERIMENTAL_UEFI_SETTINGS=false \
    DEBUG_VERBOSE=false CIX_RELEASE=unofficial STMM=unofficial
```

For Radxa 1.2.4, add `RELEASE=edk2-202608/radxa-1.2.4/unofficial` to the same
command. The default selects the active 1.3.1 line.

This builds TF-A and maintained OP-TEE, builds the hybrid StandaloneMM component,
selects `CFG_STMM_FFA_VERSION=12`, verifies the embedded payload before signing,
and assembles the complete 8 MiB `cix_flash_all.bin`. Use that complete image for
UEFI-shell testing; the OTA image is not the supported update route for this test.
`STMM=vendor` remains the default and selects FF-A mode 10.

Hybrid source contents, Radxa release, compiler identities and build helper
identify its cache;
the OP-TEE cache also includes the selected payload and handoff version. Modified
staging copies are restored before signing. The existing
`Firmwares/vendor-stmm.json` report filename is retained for build-tool
compatibility; hybrid reports identify `selection: unofficial`, the built image,
retained executables and the exact OP-TEE hash. Hybrid artifact paths and archive
names include `stmm_unofficial` to distinguish them from vendor-payload builds.

The integrated targets are the active Radxa 1.3.1 O6 line, its retained
202605/202608 checkpoints, and Radxa 1.2.4 O6 on EDK2 202608. Other historical
checkpoints need explicit integration and qualification before using this selector.
Pre-2025 bases remain explicitly rejected. This is a qualification boundary;
normal-world EDK2 does not itself select the secure-world handoff ABI.

A successful build and offline tests do not establish hardware boot. Capture the
complete UART log and test variable persistence across a cold boot, including
Secure Boot state, before treating the hybrid image as hardware qualified. The
inherited NOR/FTW power-loss recovery limitation described above remains deferred.

Both Radxa 1.2.4 and 1.3.1 O6 paths have passed full-image assembly and
vendor-anchored certificate-chain validation with EDK2 202608 in the trixie
buildbox. Each exact embedded StandaloneMM payload also passed offline
authenticated-variable and cold-persistence checks. The 1.2.4 reconstruction
matched its release-specific vendor services in all 189 sampled interruption
cases, including the same 40 inherited recovery failures.
