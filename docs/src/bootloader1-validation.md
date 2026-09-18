# Bootloader1 Validation

The public build-branch Makefile checks BL1 inputs before compilation and the
BL1 payload inside completed full flash images before mirroring or installing
them. The same checks cover staged firmware and distributable archives.
Direct invocation of a historical firmware source tree's Makefile does not
include this build-branch gate.

## Build Policy

There are two checks:

1. **Whole-file integrity, mandatory on every host.** The selected source file
   must match its committed bytes and the reviewed vendor-payload catalogue.
   The packaged BL1 must match the selected source payload. SHA-256 and length
   cover the entire BL1 file, including padding.
2. **Vendor signature verification.** The pinned CIX tool runs natively on
   x86-64 Linux. Other hosts use Docker or Podman with a pinned `linux/amd64`
   container image and the host's configured emulation support. A verification
   rejection fails the build. An unavailable download, executable, container
   runtime, or emulation setup produces a warning and the build continues.

A successful launch probe followed by a verification failure, crash, missing
success output, or timeout is a hard failure. Container launch failures are
reported separately. Warnings do not disable the whole-file integrity check.

If the verifier cannot execute, the fallback accepts **only unmodified upstream
CIX payloads recorded in the catalogue**, including CIX 2026Q1 and the CIX
payloads shipped in the retained Radxa releases. It checks the complete SHA-256
and size before attempting execution. Unknown payloads, locally rebuilt BL1s,
and any changed bytes fail even when the verifier is unavailable. A new vendor
release needs a reviewed catalogue entry; its filename or version label alone
does not grant approval.

The tool is downloaded once into `.cache/edk2-cix/bl1-vendor/` and its pinned
SHA-256 is checked before every use. `EDK2_CIX_BL1_TOOL=/absolute/path/to/tool`
can supply a pre-downloaded copy; it must match the same checksum. Container
verification has no network access, mounts its inputs read-only, and does not
receive hardware devices. No native Arm tool or source was found in the
inspected CIX repository.

Successful builds mirror `bootloader1-validation.json` alongside their raw
firmware outputs. It records the BL1 hashes, checked image paths, and whether
runtime vendor verification succeeded or was unavailable.
The `acceptance_basis` field explicitly records `approved-vendor-hash-fallback`
when execution was unavailable, together with the accepted payloads' provenance.

These checks establish preservation of qualified vendor bytes and signature
validity against the recorded reference keys. **They do not read board eFuses
or establish acceptance under a board's rollback, revocation, or lifecycle
policy.**

## Vendor Tool Investigation

On 2026-09-18, the x86-64 Linux tool was exercised in an emulated amd64
container on an Arm Mac. The source is CIX's
[BL1 tool directory](https://github.com/cixtech/cix_proprietary__cix_firmware/tree/9162ccf45c71b309cf303ec93775f2ffb1ba25bc/common/cix_tools).
It is distinct from this project's portable `cix_package_tool`, `fiptool`, and
later-stage certificate tools. It reports source commit
`fc4a3f33feb2441dc2a8208a8df36c114616c6c3`; that is a diagnostic string, not
published source code.

The published
[production configuration](https://github.com/cixtech/cix_proprietary__cix_firmware/blob/9162ccf45c71b309cf303ec93775f2ffb1ba25bc/common/cix_config/evb/cix_bl1_rsa3072_product.json)
specifies RSA-3072 PSS with SHA-256, but its named public-key files are absent.
The checked-in verification configuration retains that schema and points to
two public keys extracted from the pinned CIX 2026Q1 stock image. The keys are
fixed reference inputs; they are never taken from the candidate being checked.
No private keys are supplied or required for verification.

| Input class | Observed vendor-tool result |
| --- | --- |
| CIX 2026Q1 BL1 | Accepted |
| Seven distinct primary BL1 payloads across 22 retained Radxa releases | Accepted using the same reference key pair |
| Header, public-key, or executable-payload corruption | Rejected |
| Truncated executable payload | Rejected |
| Changes outside authenticated content, including trailing data | Not necessarily rejected |
| Unused Radxa `Firmwares2` alternatives | Different root-key identity; excluded from this catalogue |

The final two rows are why the vendor verifier alone is insufficient for the
requirement that every modified BL1 file be rejected. Whole-file preservation
and signature verification serve different purposes.

The local investigation also checked 79 retained source inputs and extracted
BL1 from three previously compiled full flash images: EDK2 202605 with Radxa
1.2.4 and 1.3.1 for O6, and EDK2 202608 with Radxa 1.2.4 for O6N. All passed.
This exercised the new validator against existing build outputs; it was not
a fresh firmware compilation or a boot test. Native x86-64 execution is
configured in CI and remains pending.

## Reproducing Qualification

From the build-branch checkout:

```bash
# Native on x86-64 Linux; emulated container on other supported hosts.
python3 scripts/qualify_bootloader1_signatures.py --download

# Explicitly use an amd64 container, including on an Arm host.
python3 scripts/qualify_bootloader1_signatures.py --download --runner docker

# Check a supplied file or extract and check BL1 inside a full flash image.
python3 scripts/validate_bootloader1.py --image /path/to/bootloader1.img
python3 scripts/validate_bootloader1.py --flash-image /path/to/cix_flash_all.bin

# Audit retained source refs without executing the vendor tool.
python3 scripts/validate_bootloader1.py --check-source-refs
```

The explicit qualification command fails if the tool cannot run: its purpose
is to demonstrate that vendor verification was actually exercised. CI supplies
`--allow-unavailable` to apply the ordinary build policy on its x86-64 Linux
runner: unavailable tools warn, while signature rejection fails. The JSON
report distinguishes these outcomes. CI also runs the mandatory source-reference
audit and regression tests.

When importing a new BL1, review its vendor provenance, qualify it against the
fixed reference keys, and deliberately update `config/bootloader1-payloads.json`.
Do not automatically approve unknown hashes, regenerate keys from a candidate,
or equate successful signature verification with board compatibility.
