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

The current `latest` stack uses EDK2 `202608`, Radxa `1.3.1`, and the CIX v1.2
early-boot replacement. That CIX input means a recorded `bootloader1.img`
payload plus CIX TF-A and OP-TEE sources used to build `bootloader2.img`; it is
not a general label for all CIX firmware source.

For a lower-level source build, choose the board and target explicitly:

```bash
make build FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE
make build FIRMWARE_BOARD=O6N FIRMWARE_TARGET=RELEASE
```

`FIRMWARE_BOARD=O6|O6N` selects the board. `FIRMWARE_TARGET=RELEASE|DEBUG`
selects the EDK2 build target. The defaults are `O6` and `RELEASE`.
`FIRMWARE_PRODUCT` defaults to `orion-o6` for O6 and `orion-o6n` for O6N so
the boards cannot overwrite each other's staged payloads or archives.

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

## Materialise A Source Tree

Most build targets render or reuse a detached cache automatically. If you want
a persistent branch for inspection or development, render it explicitly:

```bash
make render-release-branch \
  RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial \
  PERSIST=1
```

This creates a generated branch under `source/cache/release/**`. Cache branches
are disposable: the same source can be regenerated later from the retained
`source/base/**`, `source/vendor/**`, `source/port/**`,
and `source/unofficial/**` refs.

Inside a rendered firmware tree, lower-level firmware targets such as
`make -C src help`, `make -C src preflight`, or board-specific validation
targets are available. Prefer the top-level build-branch targets unless you
specifically need to work inside that rendered tree.

## Help Cache

`make help-vars` and `make help-source-targets` use the committed cache at
`config/help-cache.json` so those commands stay fast. They are read-only: they
do not rewrite the cache and they do not verify freshness every time they run.
If the cache is missing, the help helper regenerates output in memory and asks
you to refresh it.

When you change `Makefile`, `config/`, `scripts/`, or source refs that affect
the derived source-target list, update the cache explicitly:

```bash
make refresh-help-cache
make check-help-cache
```

There is no automatically installed Git pre-commit hook for this. `make test`
and CI run `make check-help-cache`, so stale committed help cache data fails
validation.

## Documentation Builds

Documentation-specific files live under `docs/` so they do not clutter the
build-branch root. Build the mdBook site with:

```bash
make docs-build
```

By default, `make docs-build` uses `DOCS_BUILD_MODE=auto`: it builds with the
host `devenv`/`cargo` toolchain when available and falls back to the
documentation container when those host tools are missing. Use
`DOCS_BUILD_MODE=host` to require a host-only build, or
`DOCS_BUILD_MODE=container` to force the container path.

The generated site is written to `.cache/edk2-cix/docs/book/html/`. Tool
downloads, cargo state, and temporary files used by the documentation build are
also kept under `.cache/edk2-cix/docs/`.
