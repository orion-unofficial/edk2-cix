# EDK2-CIX Firmware Build

This branch contains the Makefile, manifests, and scripts used to select source
versions and build firmware for the Radxa Orion O6 and O6N boards. The source
model combines:

- upstream bases such as EDK2, TF-A, and OP-TEE
- Radxa firmware source layers
- the optional CIX v1.2 early-boot replacement: a recorded `bootloader1.img`
  payload plus CIX TF-A and OP-TEE sources used to build `bootloader2.img`
- unofficial project changes
- generated materialised firmware worktrees that are ready to build

Users who want to build firmware, rather than change the source model, should
find that the `make` targets provide the flexibility they need. In this
document, a Git "ref" means a name that identifies a commit, such as a branch,
tag, or commit ID. Start with:

```bash
make help
make help-vars
make help-source-targets
```

## Licensing

Project-authored build-branch material is distributed under the GNU General
Public License, version 3; see `LICENSE`. Each retained source ref also carries
its own upstream or vendor licensing and copyright information. Those
component-specific terms continue to govern that component: the root licence
does not replace or reinterpret them.

## What does a bare `make` build?

A targetless `make` uses the `upstream` profile. It rebuilds the latest
published Radxa release from its exact EDK2 202208 source target and recorded
replay data, then compares the rebuilt payload byte-for-byte with the published
package. The current default is Radxa `1.3.1` for O6:

```bash
make
```

Select O6N explicitly:

```bash
make FIRMWARE_BOARD=O6N
```

The command reports the selected release and source target before doing any
work. On success, it reports that the strict byte comparison succeeded and
mirrors the useful raw outputs beneath `dist/build/upstream/`. It does not use
the CIX v1.2 early-boot replacement, a newer EDK2 base, or any project firmware
fix.

To build from the latest maintained source stack instead, select the `latest`
profile:

```bash
make PROFILE=latest
make PROFILE=latest FIRMWARE_BOARD=O6N
```

This currently selects EDK2 `202608`, Radxa `1.3.1`, and the CIX v1.2
early-boot replacement. It uses the project's custom-capable build path, but
keeps `ENABLE_FIRMWARE_FIXES=false`; the result is an uplifted current-source
build and is not expected to match the published 202208-based Radxa image
byte-for-byte. Enable the opinionated fixes separately:

```bash
make PROFILE=latest ENABLE_FIRMWARE_FIXES=true
```

User-facing build and packaging targets use the buildbox by default, so the
host does not need an AArch64 firmware toolchain installed.

## How do I select a source build directly?

The lower-level source-build targets default to the latest maintained source
target. For a single firmware build, choose the board and build target:

```bash
make build FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE
make build FIRMWARE_BOARD=O6N FIRMWARE_TARGET=RELEASE
```

Common variables are:

- `FIRMWARE_BOARD=O6|O6N` selects the board. The default is `O6`.
- `FIRMWARE_PRODUCT=<name>` selects the output product path and archive name.
  It defaults to `orion-o6` for O6 and `orion-o6n` for O6N.
- `FIRMWARE_TARGET=RELEASE|DEBUG` selects a release or debug firmware image.
  The default is `RELEASE`.
- `RELEASE=<source-target>` selects a configured source target. Leave this
  unset on a lower-level build target to use the latest derived source target.
- `ARTEFACT_MODE=custom|upstream` selects the build and artefact mode inside
  the chosen source tree. The default is `custom`.
- `V=1` enables verbose script and delegated build output. The default, `V=0`,
  keeps output concise.

`ARTEFACT_MODE=custom` means that the selected source tree permits project
source replacements and optional feature gates; it does not imply that
`ENABLE_FIRMWARE_FIXES` is enabled. `ARTEFACT_MODE=upstream` follows the
vendor-style path inside the selected tree. It does not select the historical
source or replay data by itself, so use the `upstream` profile or
`make deterministic-replay` when byte identity is the goal.

To stage the deployable payload under `dist/firmware/`, use:

```bash
make buildbox-firmware-stage FIRMWARE_BOARD=O6N FIRMWARE_TARGET=RELEASE
```

To create a single-board archive for the selected source target, use:

```bash
make zip FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE
make targz FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE
```

`make install` builds and stages one selected payload, then checks the selected
install root before copying anything. By default it installs under `/boot/efi`;
set `INSTALL_ROOT=/boot` or another path if your system uses a different mount
point. Existing firmware payload files under `INSTALL_ROOT` are never replaced
unless you rerun with `FORCE=1`.

```bash
make install FIRMWARE_BOARD=O6 INSTALL_ROOT=/boot/efi
make install FIRMWARE_BOARD=O6 INSTALL_ROOT=/boot/efi FORCE=1
```

`make build-all` is for producing a distributable bundle containing all
supported firmware build variants for the selected board and source target.
Here, a firmware build variant means one output selected by the rendered
firmware tree's own build matrix, not a different EDK2/CIX/Radxa source
combination. It deliberately ignores most single-image build variables because
it chooses that build-output set itself.

```bash
make build-all FIRMWARE_BOARD=O6
```

To verify that the replay-capable vendor source has not drifted away from a
published Radxa release, run the deterministic replay wrapper from this branch:

```bash
make deterministic-replay FIRMWARE_BOARD=O6
make deterministic-replay FIRMWARE_BOARD=O6N
```

By default, this renders the exact `edk2-202208/radxa-1.3.1` upstream source
target, downloads the matching `1.3.1` release package from
`radxa-pkg/edk2-cix`, and delegates to the rendered firmware tree's strict
byte-identical replay target. To replay a package you already have, pass
`REPLAY_INPUT=/path/to/edk2-cix_1.3.1_all.deb`; for a raw
`cix_flash_all.bin`, also pass `REPLAY_BUILD_OPTIONS=/path/to/BuildOptions`
when available.

The replay-capable EDK2 202208 source records timestamp, certificate, package,
and validation-profile data for Radxa `1.2.1`, `1.2.2`, `1.2.3`, `1.2.4`,
`1.3.0`, and `1.3.1`, for both O6 and O6N. Reuse is content-addressed: the
trusted-key certificate is shared across those releases because its SHA-256 is
identical, while release- and board-specific certificate data remains distinct.

## How do I choose EDK2/CIX early-boot/Radxa source versions?

A source target selects the combination of EDK2, the optional CIX early-boot
replacement, Radxa, and unofficial project sources to build. Use
`make help-source-targets` to list the configured combinations.

```bash
make help-source-targets
```

Use the `RELEASE` variable to select one of those combinations. The documented
form is the prefixless source-target name shown by `make help-source-targets`:

```bash
make build \
  RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial \
  FIRMWARE_BOARD=O6N \
  FIRMWARE_TARGET=RELEASE
```

The full internal branch name is also accepted:

```bash
make build \
  RELEASE=source/cache/release/custom/edk2-202608/cix-1.2/radxa-1.3.1/unofficial \
  FIRMWARE_BOARD=O6N
```

Build targets render or reuse a cached detached worktree. They do not normally
create or advance a named `source/cache/release/**` branch, and those branches
are treated as disposable caches rather than required source data. If you
intentionally want a persistent materialised branch for development,
inspection, or CI, set `PERSIST=1` with `make render-release-branch`:

```bash
make render-release-branch \
  RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial-1.3.1 \
  PERSIST=1
```

This creates or verifies:

```text
source/cache/release/custom/edk2-202608/cix-1.2/radxa-1.3.1/unofficial-1.3.1
```

If an explicit unofficial import changes the rendered tree, rebuild and replace
the persistent branch deliberately:

```bash
make render-release-branch \
  RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial \
  PERSIST=1 REBUILD=1 FORCE=1
```

That command also refreshes the source-target tree-ID metadata in
`config/refs-source-target-cache.json`.

A configured build variation is considered supported only when all of its
source inputs are recorded locally. At a high level, this means:

- the selected EDK2, `edk2-platforms`, and `edk2-non-osi` base refs are present
- the Radxa vendor changes are available for that EDK2 release
- any selected CIX component refs are present under
  `source/vendor/cix/<cix-release>/`
- the unofficial project source branch for the selected EDK2 release is present
- `source/base/edk2/edk2-stable*` refs declare the supported EDK2 release set
  from `202208` onward
- the chosen Radxa, CIX, and unofficial source layers are present as repo refs
- `config/refs-*.json` records the source and rendered tree IDs needed to
  verify it
- the build policy records the relevant distro defaults and replay availability

Run this to check that the derived build matrix and available refs agree:

```bash
make verify-build-matrix
```

## How are help listings kept current?

`make help-source-targets` uses an untracked runtime cache under
`.cache/edk2-cix/help/` so it can return quickly without committing generated
help data to the repository. The cache records the source refs and tracked
help-generation inputs used to build it. If those inputs change, the next help
or check invocation refreshes the cache before printing the requested output.

The first refresh in a new clone may take several seconds. When this happens,
the helper prints a `[help-cache]` status line before doing the slow work so the
command does not appear to have hung.

To refresh or validate the runtime cache explicitly, run:

```bash
make refresh-help-cache
make check-help-cache
```

`make test` also runs `make check-help-cache`. This checks the cache-generation
path, but it does not require staging or committing any cache file.

## How are source layers represented?

The firmware tree is built from several layers rather than from one permanent
monolithic branch.

`source/base/**` branches contain upstream sources, such as EDK2, TF-A, and
OP-TEE, at recorded versions. For EDK2 releases, the separate upstream `edk2`,
`edk2-platforms`, and `edk2-non-osi` repos are combined into generated
`source/cache/base/edk2/**` skeleton caches when needed. Those cache refs are
not required source data when `config/refs-edk2.json` and the referenced
component refs are present. The base-cache tree IDs are derived from that EDK2
ref metadata rather than stored in a separate manifest.

`source/vendor/cix/**` branches contain source and payload trees published by
CIX. CIX publishes OP-TEE under a `tee` directory, but this source model
records that component as `op-tee`. `source/port/cix/**` branches are reserved
for this project's reviewed ports of those components to a newer Arm upstream
base.

`source/vendor/radxa/**` branches contain source trees actually published by
Radxa for a recorded EDK2 base. `source/port/radxa/**` branches contain this
project's deterministic ports of a Radxa release to later EDK2 bases. For
example, Radxa `1.2.1` was published on `edk2-stable202208`, while
`source/port/radxa/1.2.1/edk2-stable202605` records the same vendor intent
ported forward to EDK2 `202605`.

Raw `source/vendor/**` refs preserve the vendor's materialised content and Git
file modes exactly; they are the byte-level provenance record. In editable
`source/port/**` and `source/unofficial/**` trees, files introduced or changed
by the supported integration, uplift, and import commands are canonicalised:
text uses LF line endings with no trailing horizontal whitespace, text files
without a shebang are non-executable, scripts with a shebang keep their
executable bit, and binary files are left byte-for-byte and mode-for-mode
unchanged. Untouched upstream-base files are not mass-rewritten. The split
between exact vendor refs and canonical changed files in workable refs makes a
second parallel "normalised vendor" branch namespace unnecessary.

`source/unofficial/**` branches contain this project's branded Unofficial
firmware changes as normal source trees. Mutable development tips are explicit
per release line; the sole actively maintained tip is currently
`source/unofficial/1.3/current`. The older `source/unofficial/1.2/current` tip
and its exact checkpoints remain retained historical records. Immutable uplift checkpoints use
`source/unofficial/<radxa-release>/<edk2-base>`, for example
`source/unofficial/1.3.1/edk2-stable202608`. The default line is selected by
`config/policies.json`; no unqualified `source/unofficial/current` ref is used.
The older `source/unofficial/edk2-stable*` branches and matching tags are
retained historical EDK2 compatibility records for focused source-change
propagation, not firmware-line tips.

Render plans are derived from the selected source target name and available
source refs, then verified against the ref metadata in `config/`.

Render plans can also include an explicit `materialise_submodules` step. If any
gitlinks remain after all configured steps have run, the renderer attempts
recursive submodule materialisation automatically using the nearest recorded
`.gitmodules` mapping and writes a submodule report under
`.cache/edk2-cix/reports/`. That report is mainly a diagnostic and audit aid:
it records which submodule paths were flattened, which commit IDs were used,
which URL was recorded for each submodule, and which `.gitmodules` file
supplied that mapping. You can usually ignore it when a build succeeds, but it
is useful when checking that a rendered branch contains ordinary files rather
than active submodules.

`source/cache/release/**` branches are materialised firmware branches. They are
generated from the base, component, vendor, and unofficial layers. They are
convenient to inspect or build, but they are not required source inputs: this
repository treats them as caches, and a checkout may omit them and regenerate
the selected source target from the recorded source layers and manifests.

Generated Git cache branches always live under `source/cache/**`. Use the
`make prune` target to report cache branches that are safe to remove, and
`make prune DELETE=1` to delete only verified cache branches. Use `make clean`
to remove stale transient filesystem caches, such as detached worktrees whose
source tree no longer matches the current manifests. Use `make realclean` to
remove all transient filesystem caches. Neither target deletes Git refs.

The source-target tree-ID manifest records canonical rendered trees. Versioned
unofficial aliases, such as `/unofficial-1.2.1`, are derived from the matching
canonical `/unofficial` source target for the same EDK2/CIX/Radxa combination,
and must always resolve to the same tree.

## Maintenance and validation

This default `build` branch keeps its documentation focused on selecting,
building, staging, and understanding firmware. Repository-maintenance,
upstream-uplift, source-publication, CI, and validation instructions are kept
on the `test` branch. To work on those areas, fetch that branch and read its
`MAINTENANCE.md`:

```bash
git fetch origin test
git switch test
```
