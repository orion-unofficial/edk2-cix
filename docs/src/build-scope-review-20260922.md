# Debug masks, supported scope, memory and line endings

The initial audit used build commit `5560573973` on September 22, 2026.
Subsequent integration and build results are distinguished below, with exact
source identities recorded for the logging experiment.

## Debug-mask search space

The accepted bits are read from the selected EDK2 `DebugLib.h`. Distinct numeric
masks include zero; alternate decimal/hex spellings do not add configurations.

| EDK2 releases | Independent bits | Accepted-bit union | Valid masks |
| --- | ---: | --- | ---: |
| 202208 through 202302 | 19 | `0x807B55FF` | 524,288 |
| 202305 through 202505 | 20 | `0x80FB55FF` | 1,048,576 |
| 202508 through 202602 | 21 | `0x81FB55FF` | 2,097,152 |
| 202605 and 202608 | 22 | `0x83FB55FF` | 4,194,304 |

These counts describe valid category masks, not proven deployable firmware.
On RELEASE, all 4,194,303 nonzero logging-enabled 202608 masks currently require
`FORCE_DEBUG_BUILD=1`. The final size and signature guards remain mandatory.
Ordinary `DEBUG_VERBOSE=false` defaults to `0x80000040`; the exact recent O6
202608/1.3.1 fixes-plus-menu build passed with 7,824 bytes spare in its FV.
That result does not qualify every board, menu or source combination.

The **five builds** below use the 202608 comparison base: zero, ERROR, INIT,
INFO, and ERROR|INFO, all with logging enabled. This directly separates the
categories implicated by previous size failures.

A systematic first pass is **24 builds**: zero, each of the 22 singleton
bits, and all defined bits. Add roughly four to eight useful combinations,
especially ERROR with INIT/WARN/INFO and the cheapest useful categories.
Use a fixed source, toolchain, timestamp, board, options and package layout;
record FV size and final BL33 size, retaining reproducible build receipts.
Start with 202608/1.3.1 O6, then verify candidate defaults on the other active
board/release paths. This is roughly **28–32 exploratory builds**, not millions.

If experiments identify expensive categories, an explicit conservative policy
can keep masks containing them behind the force gate. The size of those groups
is easy to calculate:

| Categories excluded from routine candidates | Masks left | Masks gated |
| ---: | ---: | ---: |
| 1 | 2,097,152 | 50% |
| 2 | 1,048,576 | 75% |
| 4 | 262,144 | 93.75% |
| 8 | 16,384 | 99.609375% |

This is a qualification policy, not proof that every superset overflows. LZMA
is deterministic for fixed input but neither linking nor compression is
monotonic in the selected mask. A singleton failure alone gives no rigorous
count of other masks that must fail. Identical preprocessed/link inputs can
establish equivalence; similar compressed sizes cannot. Store measurements
against exact build inputs, and invalidate them when those inputs change.

The historical INIT-only failure was 6,488 bytes over the **old** `0x1f2000`
FV. The current selected layout is `0x1f4000`, 8 KiB larger, and the code has
also changed. The new measurement below supersedes that historical failure.
The previous INFO|ERROR verbose result exceeded even the newer capacity by
61,856 bytes, but remains an older-source measurement.

## Five-mask build results

The five requested logging builds used the same 202608/1.3.1 source checkpoint
`33402d97a52db15ac3d6f992c2ed53c5697818ea`, O6, custom RELEASE, fixes enabled,
CIX core order, experimental settings disabled, blank `CIX_RELEASE`, trixie
and Linux arm64 build containers. `BUILD_DATE` was fixed at
`2026-09-22T12:00:00+00:00`. The compressed FV capacity was 2,048,000 bytes
(`0x1f4000`).

| Debug mask | Categories | Compressed FV bytes required | Result |
| --- | --- | ---: | --- |
| `0x00000000` | None (control) | 2,027,488 | Packaged; 20,512 bytes free |
| `0x80000000` | ERROR | 2,102,936 | Rejected; 54,936 bytes over |
| `0x00000001` | INIT | 2,043,664 | Packaged; 4,336 bytes free |
| `0x00000040` | INFO | 2,093,384 | Rejected; 45,384 bytes over |
| `0x80000040` | ERROR and INFO | 2,107,688 | Rejected; 59,688 bytes over |

Both successful full-flash images are 8 MiB and passed vendor BL1 and
certificate-chain validation. Neither result establishes a successful boot on
hardware. The zero mask emits no category messages; INIT is the only nonzero
mask in this sample which fits, with little remaining space. These measurements
apply to the stated inputs, not to every menu, fixes, board or source variant.
Nonzero masks remain behind the explicit experimental build gate.

Repeat the INIT experiment with:

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
  DEBUG_VERBOSE=true \
  DEBUG_PRINT_ERROR_LEVEL=0x00000001 \
  FORCE_DEBUG_BUILD=1 \
  BUILDBOX_PLATFORM=linux/arm64 \
  BUILD_DATE=2026-09-22T12:00:00+00:00
```

The ordinary output path is
`dist/build/edk2-202608/radxa-1.3.1/unofficial/custom+fixes/O6/RELEASE_GCC/cix_flash_all.bin`.
An overridden `BUILD_DIST_ROOT` changes the root of that path. Size guards still
run when `FORCE_DEBUG_BUILD=1`; it does not permit oversized images to package.

The measured image SHA-256 values were:

- Zero: `657b9c008072951b404ce241905149b102676baccf3376471a102302d14fb25a`.
- INIT: `d75300e0d2a07339bd3ed999b63f29f4d550416e18d7a172a2c28f08e9170f5c`.

## Practical supported scope

The initial measured gap was **336**, not 366: 360 non-CIX custom pairs, of which 24
passed the initial release-binding/vendor-payload checks. Passing those checks
still requires a source-delta audit to prove complete named-release fidelity.
Compiling the incorrectly bound aliases does not repair their inputs.

The adopted primary scope is now
`edk2-{202208,202608}/radxa-{1.2.4,1.3.1}/unofficial`. This supersedes the
initial suggestion to retain 202605 in routine qualification: 202208 represents
the vendor baseline and 202608 the current uplift. The two 202208 targets now
have separate release-correct checkpoints instead of using the generic 1.2.1
checkpoint. Both fixes and experimental-menu states are included:

- O6: four release pairs × two fixes states × two menu states = **16 builds**.
- O6N: the same **16 CI builds**; testing on hardware is currently available
  only for O6.
- Logging-enabled experiments are measured separately, with their exact mask,
  options, compressed FV size and packaging result recorded.
- Source/render/dependency checks still cover retained source checkpoints.
  Upstream reproducibility remains a separate gate.

A concrete incorrect historical alias is
`edk2-202211/radxa-1.3.1/unofficial`: its generic source checkpoint is based on
Radxa 1.2.1, and its BL1, BL2 and TrustZone configuration differ from the
requested 1.3.1 vendor payloads. A version-label edit cannot supply the missing
vendor source changes. Public builds reject it with those reasons; public help
omits it. Required refs and historical manifests are preserved.

After the two new integrations and explicit 202208 checkpoint selection, the
canonical non-CIX inventory contains 341 pairs: 26 pass the initial input
binding checks and 315 are rejected. The 22 compatible pairs outside the
primary four remain labelled as legacy, without implying full qualification.
Nineteen other incorrectly bound 202208 aliases are no longer derived from the
generic fallback. This is a selector-policy change, not source-ref deletion.

Supporting further EDK2/Radxa pairs requires real source integration and
release-specific compatibility checks; successful compilation alone cannot
establish named-release fidelity. Add those checkpoints on demand after the
primary matrix, rather than relabelling a generic historical source tree.

## O6 memory speed and Auto

Radxa's [O6 product specification](https://radxa.com/products/orion/o6/) and
[O6 documentation](https://docs.radxa.com/en/orion/o6) publish **5500 MT/s**.
This is the highest official O6 operating specification confirmed in this
review; O6N specifications must not be substituted. No universal O6 chip rating
of 6500 MT/s has been established. A memory menu's 6400 option is not a board
qualification or proof of the installed part's speed bin.

The current 1.3.1-based O6 source does expose these profile ceilings:

| O6 profile | Default upper transfer rate |
| --- | ---: |
| Ordinary 8/12/16/32 GiB, plus 32 GiB Hynix | 6000 MT/s |
| 24 GiB HS, 32 GiB x8 HS, 48 GiB HS, 64 GiB x8/Rayson | 5500 MT/s |
| 16 GiB HS and 32 GiB Low | 4800 MT/s |

Evidence is in the selected source's O6 `mem_config/RS600/GlobalConfig.c`,
`default/BoardIdMap.c`, `default/MemBiosSetup.c`, `default/QuickConfig.c`, and
`src/Makefile` (`MEM_CFG_MEMFREQ ?= 3000`, where transfer rate is twice MHz).
`Auto` is the `0xffff` frequency sentinel: leave the override unset. The board
and DRAM IDs select a profile; `DVFS_ENA` permits dynamic frequency changes.
I also decoded the `memory_config.bin` payload from the successfully built
202608 image (`a92b19b1...108a`): it contains the `0xffff` Auto sentinel and
the corresponding 3000/2750/2400 MHz profile values, plus a 2750 MHz fallback.
This confirms that these values reach the packaged image.

These files and payload establish the allowed ceiling, not the opaque memory firmware's
training outcome or instantaneous rate during UEFI/Linux. SMBIOS reporting
`MaxFreq * 2` is also not an actual clock measurement. For a specific O6, collect
its board/DRAM IDs, cold-boot training log and measured DDR/DVFS state; use those
to identify the exact profile before interpreting Auto or experimenting.

## LF policy and measured churn

Keep imported bytes and replay fixtures exact. Use LF for repository-owned
scripts/docs/new text; do not create incidental whole-file EOL changes in
upstream-derived overlays. The audit reads Git blobs, ignoring symlink targets
as independent files. NUL-containing/binary/UTF-16 data is excluded from the
simple text count and must not be blindly converted.

- Build branch: **179 text files, zero CRLF/mixed files** at the audit commit.
- Latest 202608/1.3.1: **124 outer-harness, 16 source-harness and 156 custom
  regular text files, all LF**. The 202608/1.2.4 custom files are also all LF.
- Older custom overlays retain **14 path names / 14 distinct affected blobs**
  across retained refs, with 13,872 CRLF line terminators across those unique
  blobs. A typical older checkpoint has 14 affected files and 8,514 affected
  lines (202208: 8,459); the 202605 1.3.0/1.3.1 checkpoints have one 211-line
  file. These are upstream-derived files, not repo-owned script/doc violations.
- Older `validation/replay-source/` snapshots retain CRLF by design. They are
  byte-sensitive reference data, not normalization candidates.
- Indiscriminate normalization of just the latest representative imported tree
  would touch **13,856 text-like files and 4,183,343 line terminators**, before
  considering other retained releases or UTF-16/binary fixtures.

The recommended selective policy therefore needs **no immediate cosmetic
migration**. If uniform LF is additionally desired for historical customized
files, the 14-path overlay cleanup is bounded but still causes full-line diff
churn and source-tree hash changes across releases. Do it as an explicit
separate migration with replay/build checks, not as a prerequisite for current
fixes. Do not normalize all imported repositories or OpenSSL fixtures.

## Safe custom update wrapper

Custom output now uses `custom/scripts/startup.nsh`; upstream continues using
the untouched vendor script. The custom script rejects missing inputs, captures
the updater status immediately, and returns nonzero without success/shutdown
on failure. Successful updater return retains the existing power-cycle flow
and does not claim independent readback verification.

The exporter is used by staging, install and archive paths. A custom-only
Debian post-install-staging hook changes package copies, never the vendor
source. The custom make path explicitly passes its mode through debuild's
environment sanitization; the upstream debuild arguments stay unchanged. Both execute inside rendered source trees, so the focused change was
propagated to all 28 retained Unofficial refs with matching source metadata.
All `src/` and `debian/` blob identities were preserved. Tests exercise both
packaging modes, mode changes, missing custom scripts and staging failure.

The exact script also passed seven AArch64 QEMU/UEFI Shell scenarios using
harmless EFI applications: success, device error, security violation, nonzero
warning, missing image, missing updater and invalid updater. Error returns were
respectively propagated as `0x7`, `0x1a`, `0x7`, `0xe`, `0xe`, `0x3`; none
reached the script's success/shutdown path. No vendor updater or physical flash
was executed. This does not prove that the opaque vendor tool correctly reports
every possible write failure.
