# CIX StandaloneMM

`ARTEFACT_MODE=custom CIX_RELEASE=v1.2` builds BL31 and OP-TEE from the curated
CIX sources and embeds a pinned vendor StandaloneMM image in OP-TEE. The
StandaloneMM image provides the secure SPI-NOR, UEFI variable and
fault-tolerant-write services required by the existing UEFI firmware.

Leaving `CIX_RELEASE` unset retains the selected vendor trusted firmware.
The standalone image is an embedded component, not a separately flashable
firmware update. Current O6 deployment uses the complete 8 MiB flash image
from the UEFI shell; the generated OTA image is not currently usable.

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

This work executes inside the rendered source tree because direct developer
builds do not invoke the outer build-branch caller. The helper and payload
inputs therefore belong to the retained firmware source; qualification and
report orchestration remain on `build`.

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
