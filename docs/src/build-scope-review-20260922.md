# Debug masks, supported scope, memory and line endings

Measured against build commit `5560573973` and its recorded source refs on
September 22, 2026. The custom updater fix described below follows that audit.

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

Start with **five builds** on the 202608 comparison base: zero, ERROR, INIT,
INFO, and ERROR|INFO, all with logging enabled. This directly separates the
categories implicated by previous size failures. Expand only if useful.

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
also changed. It must be rebuilt before classifying it as still oversized.
The previous INFO|ERROR verbose result exceeded even the newer capacity by
61,856 bytes, but remains an older-source measurement.

## Practical supported scope

The measured gap is **336**, not 366: 360 non-CIX custom pairs, of which 24
pass the initial release-binding/vendor-payload checks. Passing those checks
still requires a source-delta audit to prove complete named-release fidelity.
Compiling the incorrectly bound aliases does not repair their inputs.

Recommended routine catalog, subject to an explicit support-policy decision:

- Keep `edk2-{202605,202608}/radxa-{1.2.4,1.3.1}/unofficial`: four explicit
  release-specific checkpoints. Prefer 202608; retain 202605 for continuity
  with testing on hardware.
- Hide/deactivate all 336 mismatched historical aliases with a diagnostic
  identifying the mismatch and nearest supported target.
- Keep the other 20 initially valid pairs as clearly labelled legacy,
  opt-in targets: the 16 older EDK2/Radxa 1.2.1 pairs, plus 202605 with Radxa
  1.2.1, 1.2.2, 1.2.3 and 1.3.0. Do not advertise them as newly qualified.
- Preserve every required source ref, tag and historical manifest. This is
  selector/support policy, not deletion of source history.

The four primary pairs times O6/O6N times fixes off/on need **16 full builds**,
an 89% reduction from 144 and a 99% reduction from 1,440. Add a small separately
identified feature sample (experimental menus/core order/host tooling), and
keep source/render/dependency checks over every retained source. Eight jobs
would cover only 202608 with the two firmware releases; 16 is the preferable
continuity baseline. Covering all 24 initially valid pairs fully would be 96
jobs. Representative jobs give indicative coverage, not certification of all
omitted configurations. Upstream reproducibility checks remain a separate gate.

To retain every older EDK2/latest-two-Radxa combination, **32 missing pairs**
need real integration: 16 older EDK2 releases times two Radxa releases. Audit
and classify the Radxa deltas once per firmware line, replay them onto the
retained EDK2 bases, check exact source/payload binding and resolve compatibility
failures at the affected release boundaries. This shares review and tooling,
but does not eliminate the need to prove each generated checkpoint. Do this
on demand after the four current checkpoints receive a deeper content audit.
No aliases, matrices or refs were hidden/deleted by this follow-up.

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
