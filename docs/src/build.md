# Build

This repository is normally built from the `build` branch. The build branch is
an orchestration layer: it selects EDK2, CIX, Radxa, and unofficial source
versions, renders a normal firmware source tree, then delegates the firmware
build to that rendered tree.

For a quick overview, run:

```bash
make help
make help-vars
make help-source-targets
```

## Build One Firmware Image

The default behavior is a byte-identical rebuild of the latest published Radxa
firmware. A targetless invocation currently replays Radxa `1.3.1` from its
EDK2 202208 source and compares the rebuilt payload with the release package:

```bash
make
make FIRMWARE_BOARD=O6N
```

The `latest` profile instead builds from the latest maintained source stack.
It does not enable the project's opinionated fixes unless asked:

```bash
make PROFILE=latest
make PROFILE=latest ENABLE_FIRMWARE_FIXES=true
```

The current `latest` stack uses EDK2 `202608` and Radxa `1.3.1`, retaining
the matching vendor-signed BL1, BL31 and OP-TEE. Curated CIX V1.2 component
sources remain available for development, but their OEM-signed trusted FIP is
not authorised by the vendor trust root and cannot be packaged as qualified
flash firmware. See [certificate-chain validation](firmware-chain-validation.md).

For a lower-level source build, choose the board and target explicitly:

```bash
make build FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE
make build FIRMWARE_BOARD=O6N FIRMWARE_TARGET=RELEASE
```

`FIRMWARE_BOARD=O6|O6N` selects the board. `FIRMWARE_TARGET=RELEASE|DEBUG`
selects the EDK2 build target. The defaults are `O6` and `RELEASE`.
`FIRMWARE_PRODUCT` defaults to `orion-o6` for O6 and `orion-o6n` for O6N so
the boards cannot overwrite each other's staged payloads or archives.

Builds check that BL1 retains the selected vendor's exact bytes and attempt
vendor signature verification. Verification rejection fails the build;
an unavailable verifier produces a warning. See
[Bootloader1 Validation](bootloader1-validation.md) for platform support,
reports, and the limits of this check.

To build an explicit source combination, set `RELEASE` to one of the source
targets listed by `make help-source-targets`:

```bash
make build \
  RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial \
  FIRMWARE_BOARD=O6N \
  FIRMWARE_TARGET=RELEASE
```

To stage a payload under `dist/firmware/`, use:

```bash
make buildbox-firmware-stage FIRMWARE_BOARD=O6N
```

To create distributable archives for one selected board and source target, use:

```bash
make zip FIRMWARE_BOARD=O6
make targz FIRMWARE_BOARD=O6
```

`make build-all` is intended for maintainers producing a complete distributable
bundle of all supported firmware build variants for one board and source
target. Here, a firmware build variant is one output selected by the rendered
firmware tree's own build matrix, not a different EDK2/CIX/Radxa source
combination. It is broader than most single-user builds.

### Identify the image that was tested

Keep the exact invocation, repository commit, output filename, SHA-256 digest
and flashing method with a report from a test on hardware. Hash both the build
output and the file actually copied beside `startup.nsh`; keep the complete
updater and UART logs. When an existing SPI readback is available, compare it
with the intended image while accounting for known mutable regions. A stated
command, version label or successful boot alone does not prove which file was
built or written. The default O6 OTA
image (`cix_flash_ota.bin`) contains only `bootloader3.img` (UEFI). It does not
replace BL1 or BL2. A successful boot after an OTA update therefore does not
qualify a different BL1, TF-A or OP-TEE payload produced by the same build.

`BuildOptions` records EDK2's command-line defines, but does not record every
packaging option, including `CIX_RELEASE`. The firmware's version display also
does not expose a complete build configuration. Neither is sufficient by
itself to identify all components of the running firmware.

The custom firmware version header is populated at compile time as well as at
runtime. Leaving its stored string empty previously allowed the running firmware
to report a version while image-inspection tools displayed a blank new version.
A regression compiles the actual header initializer with consecutive version
selections and checks the bytes before any firmware initialization runs.

Changing supported firmware configuration switches in the same build directory
invalidates the selected board/target's previous outputs and staging files.
Regression tests toggle the switches in both directions using the source
Makefile's configuration rule and check argument forwarding through the build
wrappers. Cached compiler results and downloaded sources can still be reused.
Only use an image from a successful invocation; a failed build can leave an
older image in the mirrored output directory. Custom builds print the final
full-flash image's exact path and SHA-256 digest to identify that invocation's
output.

### Compiler warnings

EDK2's GCC compilation already uses `-Werror`. Existing upstream and platform
exceptions remain in the tool definitions and module INF files; for example,
OpenSSL demotes selected `maybe-uninitialized`, `unused-but-set-variable` and
format diagnostics. This is not a claim that every warning category is enabled
or fatal, and successful compilation is not a substitute for image validation.

In custom mode the pinned ACPICA host compiler is built with its fatal
C-warning policy. Bison also uses `-Werror`, with only the POSIX Yacc compatibility category
disabled: ACPICA needs Bison's `%expect` extension and uses `-y` for the expected
generated filenames. Other grammar warnings fail provisioning. Regression
tests exercise both cases. Custom build output preserves warnings. Upstream mode retains its existing
warning-filtering behaviour. The ASL compiler's final warning count does not
describe warnings emitted earlier while building the host tools.

## Install A Built Payload

`make install` builds and stages one selected payload, checks that the install
root is mounted read/write and has enough free space, and refuses to overwrite
existing firmware files unless `FORCE=1` is set.

```bash
make install FIRMWARE_BOARD=O6 INSTALL_ROOT=/boot/efi
make install FIRMWARE_BOARD=O6 INSTALL_ROOT=/boot/efi FORCE=1
```

Set `INSTALL_ROOT=/boot` or another mount point if your system does not use
`/boot/efi`.

## Build Modes

`PROFILE=upstream|latest` controls a targetless `make`:

- `upstream` is the default and performs exact replay of the latest published
  Radxa release
- `latest` selects the latest maintained source target, uses the custom-capable
  build path, and leaves `ENABLE_FIRMWARE_FIXES=false`

`ARTEFACT_MODE=custom` is the lower-level mode that permits the source
replacements and feature switches carried by the selected source target. It
does not enable those optional firmware fixes by itself.

`ARTEFACT_MODE=upstream` keeps the vendor-style build path for qualification
and replay checks. It is useful when comparing against a published Radxa
release, but it rejects custom-only feature variables.

For the fuller variable reference, see
[`build-variables.md`](build-variables.md).

## Replay A Published Vendor Release

Use the build-branch wrapper when you want one command to render the
replay-capable source target and run the byte-identical vendor replay build:

```bash
make deterministic-replay FIRMWARE_BOARD=O6
make deterministic-replay FIRMWARE_BOARD=O6N
```

By default, the target renders the exact upstream
`edk2-202208/radxa-1.3.1` source target, resolves the matching
release package from `radxa-pkg/edk2-cix`, downloads it under
`.cache/edk2-cix/firmware/replay/downloads/`, and delegates to the rendered
firmware tree's `deterministic-replay` target. The rendered target extracts
the vendor timestamps and signing certificate inputs, rebuilds
`ARTEFACT_MODE=upstream`, compares the rebuilt payload against the release
payload when available, and runs the strict checked-in hash validation profile.

To replay a package you already downloaded, pass it explicitly:

```bash
make deterministic-replay \
  FIRMWARE_BOARD=O6 \
  REPLAY_INPUT=/path/to/edk2-cix_1.3.1_all.deb
```

`REPLAY_INPUT` may also point at an extracted release directory or directly at
`cix_flash_all.bin`. For a raw `cix_flash_all.bin`, also provide
`REPLAY_BUILD_OPTIONS=/path/to/BuildOptions` when available, or
`REPLAY_BUILD_DATE=<iso8601>` when it is not. Set `REPLAY_DOWNLOAD=0` with no
`REPLAY_INPUT` when you intentionally want the rendered firmware target to
reuse an existing `.cache` replay input directory.

This target deliberately uses the release-specific Radxa source on the EDK2
202208 baseline even when the default build target has moved on. It overlays
the maintained build infrastructure, not unofficial firmware source. Post-202208
source targets can still use `ARTEFACT_MODE=upstream` for closest-to-upstream
diagnostics, but they are not byte-identical replays of the original published
vendor images.

The retained replay corpus covers Radxa `1.2.1` through `1.2.4`, `1.3.0`, and
`1.3.1` for both boards. Each release records its package hash, build timestamp,
certificate hashes, and strict validation profile. Identical certificate bytes
are recognized by SHA-256 instead of being treated as different merely because
they occur in different release directories.

## Repository Maintenance

Persistent materialised branches, source integration and uplift, help-cache
maintenance, documentation builds, CI, and maintainer validation are described
in `MAINTENANCE.md` at the repository root.
