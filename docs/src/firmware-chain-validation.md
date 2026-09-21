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
- complete 8 MiB flash images, pinned vendor addresses and reserved sizes,
  table bounds and payload overlap;
- full-flash and OTA members of supported ZIP and tar archives, including
  OTA destinations and signed UEFI contents.

A valid self-signature under a newly selected key is insufficient. Verification
errors, missing OpenSSL, or an unknown vendor pairing fail the build. Existing
BL1 vendor-tool verification and exact upstream hash fallback remain independent.

`firmware-chain-validation.json` reports the selected reference, image hashes,
verified payloads and counters. Host reports use the host-owned cache alongside
BL1 reports, avoiding container ownership problems. They are copied beside
published raw images. A failed check replaces any earlier successful report.

## Why Stage 3 is rejected

The curated CIX V1.2 helper signs its trusted FIP using `oem_privatekey.pem`.
That key is authorised for the non-trusted/UEFI branch, not the vendor
trusted-world branch. The vendor trusted SPKI SHA-256 is
`33d47f7435420b6216d5008fbf5ac1c2508d4326653d1550391172c68be09aa4`.
It is unchanged across retained Radxa 0.2.0 through 1.3.1 packages. The modern
OEM key is `27d49f04bf1cc4ccfc53b6e88c58624bd6e4c4e625d6e98ec9765a0f8cab07fd`.
No matching trusted-world private key was found among the retained, decodable
CIX/Radxa and development keys examined.

The delegated trusted-world, BL31-content and OP-TEE-content keys in those
packages have the same public-key fingerprint as the trusted root. None matches
the 631 private-key records examined. Reusing a stock parent certificate with
an available delegated content-signing key therefore does not provide an
alternative for these packages; the required content-signing private key is
also unavailable in the examined sources.

The tester's BL2 image 7 error identifies the Trusted Key Certificate. Image 13
is the BL31 content certificate; image 3 is BL31 itself. Parent authentication
failure can therefore produce the reported failure to load image 3.

Any nonblank `CIX_RELEASE` now fails immediately while Make parses its input,
before source preparation or output changes. An invalid invocation leaves
previous artifacts and their reports untouched; it never reports build success.
Leave `CIX_RELEASE=` to retain the selected vendor BL31/OP-TEE. The `latest`
profile and `build-all` distribution use qualified vendor trusted payloads.
The full EDK2/Radxa/board/firmware-fixes qualification matrix remains intact.
CI separately compiles both curated TF-A fix configurations with OP-TEE, then
requires the resulting FIPs to fail the vendor-root check. Development
compilation is preserved without labelling those FIPs flashable. The qualifier
invokes `src/scripts/build_cix_release_bootloader2.sh` directly, with outputs
isolated under `build-cache/untrusted-component-qualification/`; it does not
bypass the Make guard or write into the normal firmware output directory.
The imported CIX sources and the helper remain available for development.

The bundled OEM key can sign UEFI through the vendor's non-trusted delegation.
Both the retained vendor certificate and the OEM certificate presentation used
by working custom builds are checked against that externally pinned delegation.
This authority does not extend to modified BL31 or OP-TEE. Reattaching stock
certificates to changed trusted payloads fails their signed digest checks.

## Limits and further evidence

This validates the chain against retained vendor release data. It does not read
the board's fused identity, current monotonic counters or boot policy, nor prove
runtime compatibility. Reports explicitly mark board fuse acceptance as
`not-tested` and rollback state as `not-observed`.

Compare the tester's saved Stage 3 flash readback with the original built image
and working Stage 2 image offline. Retain their SHA-256 hashes, build commit,
manifest and complete UART log. No repeat Stage 3 flash is needed. NVRAM-bearing
dumps may contain private settings and should remain private.

The source-tree packaging guard is deliberately retained in Unofficial source
refs: direct `make -C src` runs inside those trees. A build-branch post-check
alone cannot stop that path publishing a bad image. CI orchestration, tests,
report collection and documentation remain on `build`.
