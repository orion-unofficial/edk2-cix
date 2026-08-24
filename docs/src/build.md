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

`ARTEFACT_MODE=custom` is the normal mode for this project. It permits the
unofficial feature switches and source overlays carried by the selected source
target.

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
`edk2-202208/radxa-<REPLAY_VERSION>` source target, resolves the matching
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
  REPLAY_INPUT=/path/to/edk2-cix_1.2.1_all.deb
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

GitHub Actions expressions such as `${{ inputs.make_target }}` and
`${{ matrix.board }}` are not Makefile variables. They are values that the
GitHub Actions runner normally supplies from the event payload and from the
workflow matrix. The local `act` wrapper exposes the common controls through
Makefile variables:

- Use `ACT_JOB=<job-id>` to run one job from the selected workflow. The job IDs
  are shown by `make gha-act-list`.
- Use `ACT_MATRIX=<name:value>` for matrix values such as
  `${{ matrix.board }}`. Without a matrix filter, `act` may run every matrix
  entry for the selected job.
- Use `ACT_EXTRA_ARGS='--input name=value ...'` for `workflow_dispatch` inputs
  such as `${{ inputs.make_target }}` or `${{ inputs.board }}`. The workflows
  define defaults for their inputs, but passing explicit values makes local
  runs easier to understand and reproduce.
- Use `ACT_EXTRA_ARGS='--input-file path'` instead when you prefer to keep
  several inputs in a file. `act` reads the file in `name=value` format.

For example, `firmware-build.yaml` uses `workflow_dispatch` inputs rather than
a matrix:

```bash
make gha-act-dry-run \
  ACT_WORKFLOW=.github/workflows/firmware-build.yaml \
  ACT_JOB=firmware \
  ACT_EXTRA_ARGS='--input make_target=buildbox-firmware-stage --input board=O6 --input firmware_target=RELEASE --input artefact_mode=custom'
```

The same command can be executed for real by replacing `gha-act-dry-run` with
`gha-act-run`.

For workflows with board matrices, select one matrix entry with `ACT_MATRIX`.
For example:

```bash
make gha-act-dry-run \
  ACT_WORKFLOW=.github/workflows/secure-boot-audit.yaml \
  ACT_JOB=secure-boot \
  ACT_MATRIX=board:O6
```

Some workflows use both workflow inputs and a matrix. In that case, combine
`ACT_MATRIX` and `ACT_EXTRA_ARGS`. For example:

```bash
make gha-act-dry-run \
  ACT_WORKFLOW=.github/workflows/deterministic-replay.yaml \
  ACT_JOB=replay \
  ACT_MATRIX=board:O6 \
  ACT_EXTRA_ARGS='--input replay_source_target=edk2-202208/radxa-1.2.1 --input replay_version=1.2.1 --input upstream_repository=radxa-pkg/edk2-cix'
```

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
