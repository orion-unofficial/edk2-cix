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
| `CIX_RELEASE=unofficial STMM=unofficial` | Reserved for the reconstructed payload; currently rejected |

`STMM=vendor` is the default. Source-built trusted firmware remains restricted
to custom O6 builds with the reviewed signing and pinned-payload integration.
The new selector is available on the active 1.3 line and the Radxa 1.2.4/1.3.1
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
Both configurations have compiled in the Linux buildbox. Focused C tests
exercise the actual helpers with mocked hardware; they do not establish boot
or variable-service compatibility. The modern compile check uses a synthetic,
nonbootable input, not a distributable firmware image.

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

## Deferred source reconstruction

Source reconstruction is outstanding and is gated on a confirmed successful
hardware boot with the pinned vendor image. Keep the full-flash image hash,
component verification reports, exact source target and serial boot log with
that qualification. Compilation and certificate validation do not establish
successful boot or variable persistence.

After that gate, reconstruct the CIX StandaloneMM DSC/FDF and replace components
from public source where their interfaces and behaviour are established. Retain
vendor binaries where a replacement would depend on uncertain contracts.

The public EDK2 MM core, CPU, variable and fault-tolerant-write sources are
available, but exact vendor configuration and CIX boot adaptations require
review. The CIX secure NOR, serial-output and firmware-version implementations
are absent as source in the inspected tree; their recovered executables are
36, 16 and 20 KiB respectively. Their communication layouts, HOB/PCD usage,
status conventions and storage semantics must be established before replacement.

The CIX OP-TEE loader and service bridge and the TF-A xSPI transport already
exist as source. The immediate integration restores their missing input;
reconstruction must not invent a second flash-controller owner. Modern EDK2
boot-information/HOB interfaces also need explicit adaptation to the older
CIX OP-TEE handoff. Do not mix checkpoints or replace the entire standalone
core merely because an executable has the same name.

Qualification of a reconstructed or hybrid image must cover cold and warm boot,
variable reads/writes, authenticated variables, persistence, fault-tolerant
updates, malformed requests and storage-error propagation. Preserve the pinned
vendor image as the comparison baseline until the replacement is qualified.
Before enabling `STMM=unofficial`, verify the normal-world UEFI client contract
as well as the secure-side entry/CPU contract. In particular, establish the
202208 baseline's session parameters, communication-buffer layout, return
statuses and variable-service behaviour against the rebuilt secure components.
If a pre-2025 base cannot be supported with that evidence, reject that exact
combination explicitly. The current blanket rejection of `STMM=unofficial`
applies to pre-2025 and newer bases alike.
