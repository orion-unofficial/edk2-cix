# BL33 loader and flash-layout constraints

## Supported loader and scope

The adaptive layout is restricted to the exact Radxa 1.3.1 BL1 payload identified
below. Its BL2 consumes the BL33 address and length from the outer flash table;
no fixed `0x5ff000` cutoff was found in the traced read path. The physical flash
bound is 8 MiB. Firmware has a stricter loaded-FD RAM bound of
`0x84400000` through `0x84800000`, where the GOP framebuffer begins.

[Adaptive sizing](debug-layout.md) enforces both bounds and measures the signed
FIP before selecting an allocation. Offline build/packaging checks have passed;
this static loader analysis and the isolated routine tests do not establish
successful booting of an enlarged image on hardware.

## Flash map

| Region | Radxa 1.3.1 offset / extent |
| --- | --- |
| BL1 package | `0x188000`, reserved `0x100000` |
| BL31 / OP-TEE FIP | `0x288000`, reserved `0x100000` |
| UEFI variables / FTW working / FTW spare | `0x388000` through `0x400000` |
| Memory configuration | `0x400000` through `0x404000` |
| PM configuration | `0x404000` through `0x405000` |
| SE configuration | `0x405000` through `0x406000` |
| BL33 FIP | `0x406000`, reserved `0x1f9000`, end `0x5ff000` |
| Unallocated package tail | `0x5ff000` through `0x800000` |

The inspected complete 8 MiB image contains only `0xff` in its 2,101,248-byte
tail. No later payload appears in the vendor packaging JSON. O6 defines
`SPI_VARIABLE_BASE=0x388000`, `SPI_VARIABLE_SIZE=0x28000`; the three stores
occupy the earlier `0x78000` reservation, not the tail. This describes the
packaged image and source consumers, not every possible live-flash state or
undocumented use inside opaque vendor components.

The vendor's **Radxa 1.2.4** layout already assigns BL33 `0x3fe000` through
`0x800000`: a `0x402000` slot. Thus using the top of this chip for BL33 is not
an unprecedented layout concept. The 1.3.1 BL33 start is 32 KiB later. Keep
release-specific layouts separate: this review does not qualify 1.2.4's BL2.

At the 1.3.1 start, the whole physical remainder is `0x3fa000` = 4,169,728
bytes. A fixed `0x400000` FD cannot fit there, even before FIP certificates and
headers. The build therefore measures compressed content and uses a smaller
block-aligned FD where possible. The final signed FIP, not an assumed certificate
overhead, must fit the selected allocation.

## Evidence from the exact signed BL2

Examined `bootloader1.img` SHA-256:
`fcddd093649e243b16e0b9f92ec3bec7e68e03fe78fed2c594952f0eefd72f15`.
BL2 starts at file offset `0xa4000`; its declared payload length is `0x2f729`.
Offsets below refer to this complete file, not runtime addresses.

- At `0xa850c`, BL2 reads the outer flash table, then requests type 2 and type 7.
  Type 7 is the BL33 FIP. The table-lookup leaf at `0xa84bc` copies the matching
  entry's address **and length** into its I/O block descriptor.
- At `0xa861c`, successful lookup retains those values. Only the failed-lookup
  path supplies fallback addresses (`0x288000` and `0x3f0000`). The descriptor's
  initial `0x300000` lengths are defaults, not proof of a hard three-MiB limit.
- The BL33 descriptor at `0xd3350` identifies image 5. Its `image_info_t` at
  `0xd3358` has base `0x84400000`, initial image size zero, and maximum size
  `0x800000`. The later entry-point field repeats `0x84400000`.
- At `0xa8afc` through `0xa8b20`, the loader checks the FIP entry's image length
  against that maximum before reading it. Authentication follows the read.
- The flash read routine at `0xa70ec` checks base + position + requested length
  against the flash device's size. This matches the retained TF-A `io_mtd.c`
  device-bound check, rather than enforcing the packaging JSON's slot end.

The exact 80-byte table-lookup leaf was run in an Arm container against synthetic
in-memory headers. It returned the requested type-7 lengths unchanged for
`0x1f9000`, `0x210000`, `0x280000` and `0x3fa000`, and rejected an absent type.
The exact read-bounds routine at `0xa70ec` was also executed with a mocked
read callback and an 8 MiB device descriptor. It accepted reads crossing
`0x5ff000`, starting at `0x600000`, and ending exactly at `0x800000`; it
returned `-EINVAL` for a read extending past `0x800000`. This confirms that
routine checks physical device bounds rather than the old slot end.

These tests perform no hardware I/O and do not exercise real SPI reads,
signatures, DDR mappings, decompression or the complete boot chain.

The older public CIX TF-A headers are useful context but are not the authority
for a later opaque BL2. The actual 1.3.1 binary above is the stronger evidence.
Upstream describes the general [FIP loading model](https://trustedfirmware-a.readthedocs.io/en/latest/getting_started/tools-build.html);
platform loading limits still need checking separately.

## What signatures protect

The outer flash table is outside the signed BL1 payload and the signed FIP
certificate contents. Changing its type-7 length does not intrinsically alter
the byte-identical BL1/BL2 payloads or their signatures. The changed BL33 must
still be packaged and signed through the existing delegated UEFI key chain.
The private key for the trusted BL31/OP-TEE branch is still unavailable.

There is no identified later payload to relocate for this layout. Relocating
other objects would not necessarily invalidate their content signatures, but
could break hard-coded consumers and update/recovery expectations. It is not
needed for a moderate BL33 increase.

Appending bytes while pretending the image still has its old length is not a
sound build format. The FIP entry lengths, outer flash entry length, FD/FV
sizes, signatures and packaging bounds should all describe the real image.
Some reader layers may not enforce the same bound, but exploiting that mismatch
would create inconsistent updates and validation. Keep the size checks.

## Remaining qualification

Testing on hardware still needs programmer recovery, uninterrupted UART capture,
full-image readback and cold/warm boots. The opaque full-image updater may have
constraints not visible in these isolated loader tests. The enlarged format
intentionally omits OTA output. No second-stage BL33 shim is implemented or
needed by the current direct-loading design.

## Boot-progress category change

The custom overlay now adds BM to five live INFO BDS messages and four
INFO/LOAD boot-selection messages, and to the CIX/Radxa platform progress calls.
Two of those four boot-selection messages (the boot-option description or
unknown-device-path message) remain inside `DEBUG_CODE_BEGIN/END`, so they are
absent from logging-only RELEASE. The compiled binary retains the other two,
the five BDS messages, the three platform messages and the direct BDS markers.
Existing INFO, LOAD and INIT membership is retained; error categories and
DEBUG_CODE gating are unchanged. All changes are in BL33.

`BdsEntry.c` and `BmBoot.c` exactly matched the retained upstream EDK2 202608
files before retagging. Platform `PlatformBm.c` is the CIX/Radxa adaptation of EDK2's Arm platform
boot manager. All three retagged INFO messages also exist in upstream
`ArmPkg/Library/PlatformBootManagerLib/PlatformBm.c`; their INFO classification
is inherited from EDK2. Its targeted direct INIT breadcrumbs were earlier
additions by this project.
The new overlays retain each EDK2 release's own source and mirror all companion
module inputs. Imported trees and `ARTEFACT_MODE=upstream` stay unchanged.

The compiled-marker checker accepts INIT or BM and runs inside the delegated
firmware build. See
[logging behavior and measured builds](debug-layout.md) for the current settings
and qualification limits.
