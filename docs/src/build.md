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

For the latest supported source target, choose the board and target:

```bash
make build FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE
make build FIRMWARE_BOARD=O6N FIRMWARE_TARGET=RELEASE
```

`FIRMWARE_BOARD=O6|O6N` selects the board. `FIRMWARE_TARGET=RELEASE|DEBUG`
selects the EDK2 build target. The defaults are `O6` and `RELEASE`.

To build an explicit source combination, set `RELEASE` to one of the source
targets listed by `make help-source-targets`:

```bash
make build \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/unofficial \
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

`ARTEFACT_MODE=custom` is the normal mode for this project. It permits the
unofficial feature switches and source overlays carried by the selected source
target.

`ARTEFACT_MODE=upstream` keeps the vendor-style build path for qualification
and replay checks. It is useful when comparing against a published Radxa
release, but it rejects custom-only feature variables.

For the fuller variable reference, see
[`build-variables.md`](build-variables.md).

## Materialise A Source Tree

Most build targets render or reuse a detached cache automatically. If you want
a persistent branch for inspection or development, render it explicitly:

```bash
make render-release-branch \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/unofficial \
  PERSIST=1
```

This creates a generated branch under `source/cache/release/**`. Cache branches
are disposable: the same source can be regenerated later from the retained
`source/base/**`, `source/vendor/**`, `source/port/**`, `source/component/**`,
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

The generated site is written to `docs/book/html/`.

## Test GitHub Actions Locally

Use the repo-managed `act` wrapper before pushing workflow changes:

```bash
make gha-act-list
make gha-act-dry-run \
  ACT_WORKFLOW=.github/workflows/upstream-versions.yaml \
  ACT_JOB=upstream-versions
make gha-act-run \
  ACT_WORKFLOW=.github/workflows/upstream-versions.yaml \
  ACT_JOB=upstream-versions
```

The wrapper downloads a pinned `act` binary under `.cache/edk2-cix/tools/act/`
and keeps its cache under `.cache/edk2-cix/act-cache/`. Set `ACT_WORKFLOW`,
`ACT_EVENT`, `ACT_JOB`, `ACT_MATRIX`, `ACT_SECRET_FILE`, or `ACT_EXTRA_ARGS`
when a workflow needs more specific inputs.

## Validation

For normal firmware building, the selected build target performs the necessary
preflight checks. When changing source refs, render logic, manifests, or CI,
run:

```bash
make test
make lint
make verify-build-matrix
make verify-manifest-integrity
make check-ref-integrity
make verify-minimised-clone
make check-help-cache
make verify-identity-integrity
make check-vendor-workflow-drift
make check-upstream-versions
```

Run `make refresh-help-cache` before `make check-help-cache` when your changes
alter help text or the derived source-target list.
