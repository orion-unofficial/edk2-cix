# Custom BL33 sizing and logging policy

## Build-time selection

Custom builds accept every mask composed of the selected EDK2 release's defined
`DEBUG_*` bits. They do not infer final size from singleton measurements.
After compilation, an audit of the actual compiler dependency files warns about
selected categories without labelled source diagnostics. It retains every bit.
Dynamic/all-level logging and opaque vendor modules limit what that audit proves.

The builder starts with the normal RELEASE FD capacity, including when measuring
a full DEBUG build. A recognised outer compressed-FV overflow triggers a rebuild
with a minimally larger, block-aligned FD. The generated FDF is private build
state; vendor and overlay source files are never rewritten. Changing the size
PCD can change compression, so retries remeasure rather than assume convergence.
Compiler failures are not size retries. The signed FIP is the final size check.

For the audited Radxa 1.3.1 loader and layout:

| Actual signed BL33 FIP size | Result |
| --- | --- |
| Up to `0x1f9000` | Original allocation at `0x406000`, ending `0x5ff000` |
| Above `0x1f9000`, up to `0x3fa000` | Requires `DEBUG_ALLOW_LARGE_IMAGE=1`; allocation ends at `0x800000` |
| Above `0x3fa000` | Build fails, including with opt-in |

The flash table retains the actual FIP payload length, as the vendor format
requires. The flash slot is padded with `0xff`; padding is not appended inside
the FIP or included as another authenticated executable. Full images remain
exactly 8 MiB. An opt-in small build still uses the original layout.

There is a separate RAM bound: the FD loads at `0x84400000`, below the GOP
framebuffer at `0x84800000`. Both the adaptive builder and chain validator limit
the loaded FD to 4 MiB. FIP certificates count against flash space but are not
part of this loaded FD.

The extension is pinned to the exact audited BL1 and original layout. Other
loaders retain their existing allocation until reviewed. In particular, Radxa
1.2.4 already reserves through the chip's end, so using more of its original slot
does not require permission to change its layout. No earlier region moves.

An enlarged 1.3.1 layout emits `cix_flash_all.bin`, its layout report and ordinary
build evidence, but no OTA package. Use the existing full-image `startup.nsh`
update path after independent recovery planning and testing on hardware.
Build validation does not establish hardware boot acceptance.

## Logging behavior

| Change | Current effect |
| --- | --- |
| Logging-only RELEASE header | `DEBUG_VERBOSE=true` re-enables `DEBUG()` while retaining `MDEPKG_NDEBUG` and `NDEBUG` |
| Nonlogging debug helpers | Assertions, `DEBUG_CODE`, memory fills and code guarded by those RELEASE definitions remain disabled |
| Print property mask | Verbose RELEASE enables printing with `PcdDebugPropertyMask=0x02`; board defaults cannot silently disable it again |
| Fixed print-level mask | Verbose RELEASE compiles out categories excluded by `DEBUG_PRINT_ERROR_LEVEL` |
| Defaults | `DEBUG_VERBOSE=false`: `0x80000040`; true without an explicit mask: every defined category (`0x83fb55ff` on 202608) |
| Direct progress diagnostics | Existing BDS/setup breadcrumbs provide targeted progress without enabling all DEBUG code |
| Boot-manager categorisation | BM was added alongside existing INFO/LOAD/INIT membership; original categories and DEBUG_CODE gating remain |
| Compiled-output guard | Requested INIT/BM BDS markers must actually survive into `BdsDxe.efi` |
| Initial FD capacity | Custom 1.3.1 starts at `0x1f4000`, within the original slot; adaptive sizing increases it only when needed |

This policy keeps verbose logging separate from full `FIRMWARE_TARGET=DEBUG`
semantics. Printing still adds format strings, argument evaluation, calls and
runtime I/O cost; it cannot be cost-free. No string dictionary or replacement
compression format has been introduced. Existing firmware compression remains.

`DEBUG_ALLOW_LARGE_IMAGE` is included in the configuration stamp and the
experimental System Information rebuild recipe. Changes invalidate stale custom
outputs. `ARTEFACT_MODE=upstream` does not use these sizing/logging helpers and
does not permit enlarged layouts.

## Qualification on 2026-09-23

Four sequential builds passed the expected outcomes in one workspace and cache.
The configuration was O6, EDK2 202608, Radxa 1.3.1, custom RELEASE, firmware fixes
enabled, CIX core order, experimental menus disabled and `DEBUG_VERBOSE=true`.
`CIX_RELEASE` was blank. The native Arm64 Trixie container used GCC 14.2.0 with
`-Werror`, Python 3.13.5 and OpenSSL 3.5.7. `BUILD_DATE` was fixed to
`2026-09-23T00:00:00+00:00`.

| Case | Mask | Large-image consent | FD bytes | Signed FIP bytes | FV spare bytes | Result |
| --- | --- | --- | --- | --- | ---: | --- |
| Small | BM `0x400` | No | `0x1f4000` | `0x1f52d3` | 6,216 | Original layout, full image and OTA |
| Large, denied | All `0x83fb55ff` | No | `0x204000` | `0x2052d3` | 2,312 | Correctly rejected; no published image |
| Large, enabled | All `0x83fb55ff` | Yes | `0x204000` | `0x2052d3` | 2,312 | Enlarged layout, full image only |
| Small again | BM `0x400` | Yes | `0x1f4000` | `0x1f52d3` | 6,216 | Original layout and OTA restored |

The large cases each required one measured FD enlargement after the initial
compressed-FV overflow. Captured generated FDFs and PrePi settings confirm that
the loaded FD size changed with the build and stayed below the framebuffer.
Compiled BDS flags retained `MDEPKG_NDEBUG` and `NDEBUG`: these were logging-only
RELEASE builds, not full DEBUG builds.

Independent checks revalidated the saved full images and eligible OTA images,
their certificate chains, exact payload sizes, flash bounds and erased padding.
Every full image is exactly 8 MiB. Every non-BL33 payload is byte-identical across
the three successful cases, including vendor BL1 and the trusted-firmware FIP.
The final small FD is byte-identical to the first small FD. Their signed FIPs
and full images differ because BL33 certificates and signatures were regenerated;
the executable FD, certificate public keys and certificate extensions match.

The dependency audit read 1,761 compiler dependency files covering 3,421 source
and header files, with no unreadable inputs. It warned about CACHE, FS, LOADFILE,
MANAGEABILITY, SECURITY, UNDI and VARIABLE lacking labelled diagnostics in that
source set. All requested bits were retained. This does not prove those bits
have no effect through dynamic/all-level calls or opaque vendor modules.

The all-category build invocation is:

```sh
make build \
  RELEASE=edk2-202608/radxa-1.3.1/unofficial \
  ARTEFACT_MODE=custom \
  FIRMWARE_BOARD=O6 \
  FIRMWARE_TARGET=RELEASE \
  FIRMWARE_DISTRO=trixie \
  ENABLE_FIRMWARE_FIXES=true \
  ENABLE_CORE_ORDER=cix \
  ENABLE_EXPERIMENTAL_UEFI_SETTINGS=false \
  DEBUG_VERBOSE=true \
  DEBUG_ALLOW_LARGE_IMAGE=1 \
  CIX_RELEASE=
```

Leave `DEBUG_PRINT_ERROR_LEVEL` unset to select every defined category. The
qualified image is an offline build/packaging result: **the enlarged layout has
not been tested on hardware**. These measurements do not qualify another board,
release, toolchain, menu configuration or full `FIRMWARE_TARGET=DEBUG` build.

The measured source checkpoint was
`0ea414845811284204266d1cd70ba5dfb42480e5` on
`source/unofficial/1.3.1/edk2-stable202608`. These are configuration-specific
measurements, not byte-size promises for future revisions. Ordinary build
outputs are mirrored under
`dist/build/edk2-202608/radxa-1.3.1/unofficial/custom+fixes/O6/RELEASE_GCC/`;
`BUILD_DIST_ROOT` can override the output root. Keep the image hash and validation
reports with any test result.
