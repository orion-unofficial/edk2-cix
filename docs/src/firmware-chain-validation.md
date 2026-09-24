# Firmware certificate-chain validation

A compiled image can still fail before UEFI starts. For `ARTEFACT_MODE=custom`,
the public Makefile checks
selected source inputs before compilation and checks output images before
mirroring them to `dist/`. The rendered tree also checks its staged payloads
before full-flash or OTA packaging, and checks the full-flash candidate before
renaming it to `cix_flash_all.bin`. These guards are independent of
`FIRMWARE_VALIDATE_ON_BUILD` and `ENABLE_FIRMWARE_FIXES`. They do not execute
on `ARTEFACT_MODE=upstream`; vendor replay keeps its existing checks and warning
filtering unchanged.

## What is checked

The versioned `config/firmware-trust.json` catalogue pins matching vendor BL1,
trusted FIP and UEFI delegation-certificate triples. Each entry records immutable
vendor commits. Regression tests prove the hashes against those commits.

The validator checks:

- BL1 identity and its pairing with the selected vendor boot chain;
- FIP table bounds, uniqueness, overlap and required members;
- RSA-PSS/SHA-256 certificate signatures with native OpenSSL;
- trusted-world and UEFI key delegation against the pinned vendor reference;
- signed hashes of BL31, OP-TEE, UEFI and any optional configuration payloads;
- consistent rollback counters at least as high as the reference;
- complete 8 MiB flash images, pinned vendor addresses and allowed reserved
  sizes, table bounds and payload overlap; the audited custom BL33 extension
  additionally requires [large-image consent](debug-layout.md);
- full-flash and OTA members of supported ZIP and tar archives, including
  OTA destinations and signed UEFI contents.

A valid self-signature under a newly selected key is insufficient. Verification
errors, missing OpenSSL, or an unknown vendor pairing fail the build. Existing
BL1 vendor-tool verification and exact upstream hash fallback remain independent.

`firmware-chain-validation.json` reports the selected reference, image hashes,
verified payloads and counters. Host reports use the host-owned cache alongside
BL1 reports, avoiding container ownership problems. They are copied beside
published raw images. A failed check replaces any earlier successful report.

## Published CIX signing keys and source-built trusted firmware

CIX published the [full Sky1 PackageTool key set at commit
`94368c7c2f254528634fdb518e21545bf6c8f0b1`](https://github.com/cixtech/edk2-non-osi/tree/94368c7c2f254528634fdb518e21545bf6c8f0b1/Platform/CIX/Sky1/PackageTool/Keys).
The published `cix_privatekey.pem` has the same public-key SHA-256 fingerprint
(`33d47f7435420b6216d5008fbf5ac1c2508d4326653d1550391172c68be09aa4`)
as the trusted-world anchor pinned for the retained Radxa packages. This
resolves the previous Stage 3 helper defect: it used the UEFI OEM key as the
trusted-firmware root, which could never authenticate under stock BL1.

For custom `CIX_RELEASE=1.2`, the helper now uses the published CIX root,
trusted-world delegation, BL31 and OP-TEE content keys for their respective
certificates. It retains the byte-identical CIX 2026Q1 BL1 image. The preflight
pins all copied key files, confirms the root against the selected stock
reference and confirms the CIX BL1 hash. The post-build validator independently
verifies the FIP signatures, signed executable digests, rollback counters and
flash layout. Source-built OP-TEE trusted-application signing uses the
published CIX OEM key; the Radxa UEFI signing path retains its existing OEM
key and certificate chain. `ARTEFACT_MODE=upstream` is unchanged.

CI compiles and checks both curated TF-A fix configurations using
`scripts/qualify_source_trusted_firmware.py`; its report and test images stay
under `build-cache/trusted-component-qualification/`. The normal
`CIX_RELEASE=1.2` Make path must also pass the image guards before it can be
published. A `/cix-1.2/` source lineage in `RELEASE` alone does not activate
this option.

These private keys are public. A valid signature from them does not prove that
only CIX produced the code. Offline validation also does not establish a
particular board's fuse identity, rollback state or runtime compatibility.
The [independent O6 BL1 re-signing report](https://github.com/Neol00/edk2-cix-unlocked/issues/12#issue-5555789220)
is encouraging hardware evidence, but it does not by itself qualify a
particular full-image build or other boards. Preserve a known-working
programmer recovery image for any on-board experiment.

## Limits and further evidence

This validates the chain against retained vendor release data. It does not read
the board's fused identity, current monotonic counters or boot policy, nor prove
runtime compatibility. Reports explicitly mark board fuse acceptance as
`not-tested` and rollback state as `not-observed`.

When diagnosing a failed image, compare an existing flash readback with the
original build and a known-working image offline. Retain SHA-256 hashes, the
build commit, manifest and complete UART log. Do not repeat a known-invalid
flash merely to collect the same authentication failure. NVRAM-bearing dumps
may contain private settings and should remain private.

The source-tree packaging guard is deliberately retained in Unofficial source
refs: direct `make -C src` runs inside those trees. A build-branch post-check
alone cannot stop that path publishing a bad image. CI orchestration, tests,
report collection and documentation remain on `build`.
