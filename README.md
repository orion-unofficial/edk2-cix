# EDK2-CIX Firmware Build

This branch contains the Makefile, manifests, and scripts used to select source
versions and build firmware for the Radxa Orion O6 and O6N boards. The source
model combines:

- upstream bases such as EDK2, TF-A, and OP-TEE
- Radxa and CIX vendor source layers
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

## How do I build the latest firmware?

The default source target is derived from the latest available supported EDK2
release, CIX release, Radxa release, and unofficial source branch. The
supported source-target set is generated from source refs such as
`source/base/edk2/**`, `source/vendor/radxa/**`, `source/port/radxa/**`,
`source/component/cix/**`, and the source/ref manifests under `config/`. If you
do not set `RELEASE=...`, the build targets use that derived default source
target. User-facing build and packaging targets use the buildbox by default, so
the host does not need an AArch64 firmware toolchain installed.

For a normal single firmware build, choose the board and build target:

```bash
make build FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE
make build FIRMWARE_BOARD=O6N FIRMWARE_TARGET=RELEASE
```

Common variables are:

- `FIRMWARE_BOARD=O6|O6N` selects the board. The default is `O6`.
- `FIRMWARE_TARGET=RELEASE|DEBUG` selects a release or debug firmware image.
  The default is `RELEASE`.
- `RELEASE=<source-target>` selects a configured source target. Leave this
  unset to use the latest derived source-target.
- `ARTEFACT_MODE=custom|upstream` selects the build and artefact mode inside
  the chosen source tree. The default is `custom`.
- `V=1` enables verbose script and delegated build output. The default, `V=0`,
  keeps output concise.

Most users should leave `ARTEFACT_MODE=custom`. It enables the unofficial
firmware build switches exposed by this project. `ARTEFACT_MODE=upstream` is
for vendor-style comparison or qualification builds; it does not change the
selected source target, and it rejects unofficial custom-only feature
variables. Use `RELEASE=...` to choose source versions.

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

Exact-replay comparisons can provide `SIGNING_CERT_SOURCE_DIR=<path>`. This is
a maintainer-oriented option documented by `make help-dev`; normal firmware
builds do not need it.

## How do I choose EDK2/CIX/Radxa source versions?

A source target selects the combination of EDK2, CIX, Radxa, and unofficial
project sources to build. Use `make help-source-targets` to list the configured
combinations.

```bash
make help-source-targets
```

Use the `RELEASE` variable to select one of those combinations. The documented
form is the prefixless source-target name shown by `make help-source-targets`:

```bash
make build \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/unofficial \
  FIRMWARE_BOARD=O6N \
  FIRMWARE_TARGET=RELEASE
```

The full internal branch name is also accepted:

```bash
make build \
  RELEASE=source/cache/release/custom/edk2-202602/cix-1.2/radxa-1.2.1/unofficial \
  FIRMWARE_BOARD=O6N
```

Build targets render or reuse a cached detached worktree. They do not normally
create or advance a named `source/cache/release/**` branch, and those branches
are treated as disposable caches rather than required source data. If you
intentionally want a persistent materialised branch for development,
inspection, or CI, set `PERSIST=1` with `make render-release-branch`:

```bash
make render-release-branch \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/unofficial-1.2.1 \
  PERSIST=1
```

This creates or verifies:

```text
source/cache/release/custom/edk2-202602/cix-1.2/radxa-1.2.1/unofficial-1.2.1
```

If an explicit unofficial import changes the rendered tree, rebuild and replace
the persistent branch deliberately:

```bash
make render-release-branch \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/unofficial \
  PERSIST=1 REBUILD=1 FORCE=1
```

That command also refreshes the source-target tree-ID metadata in
`config/refs-source-target-cache.json`.

A configured build variation is considered supported only when all of its
source inputs are recorded locally. At a high level, this means:

- the selected EDK2, `edk2-platforms`, and `edk2-non-osi` base refs are present
- the Radxa vendor changes are available for that EDK2 release
- any selected CIX component refs are present under
  `source/component/cix/<cix-release>/`
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

`source/component/cix/**` branches contain CIX-provided components. CIX
publishes OP-TEE under a `tee` directory, but this source model records that
component as `op-tee`.

`source/vendor/radxa/**` branches contain source trees actually published by
Radxa for a recorded EDK2 base. `source/port/radxa/**` branches contain this
project's deterministic ports of a Radxa release to later EDK2 bases. For
example, Radxa `1.2.1` was published on `edk2-stable202208`, while
`source/port/radxa/1.2.1/edk2-stable202602` records the same vendor intent
ported forward to EDK2 `202602`.

`source/unofficial/**` branches contain this project's unofficial firmware
changes as normal source trees. Release-specific branches such as
`source/unofficial/edk2-stable202602` are release branches known to
build with a selected EDK2 release. `source/unofficial/current` is the current
development branch.

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

## How do I start developing on this codebase?

1. Choose the source target you want to develop against by running
   `make help-source-targets`.
2. Materialise that source target.
3. Create a normal topic branch from the materialised firmware branch.
4. Build and test your change.
5. Import the finished change back through `source/unofficial/current` with
   `make import-changes`.

Example:

```bash
make render-release-branch \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/unofficial \
  PERSIST=1
git switch -c my-change source/cache/release/custom/edk2-202602/cix-1.2/radxa-1.2.1/unofficial
```

The supported workflows guard `source/base/**`, `source/vendor/**`,
`source/port/**`, and `source/component/cix/**` refs by comparing them against
the expected object IDs and tree IDs in `config/refs-*.json`. Git itself does
not make those branch names immutable, so do not edit or move them by hand. If
a guarded ref moved unexpectedly, or is checked out in a dirty worktree, the
scripts abort before using it. Only `make integrate-source-release` should
create or advance those refs.

## How do I persist development changes back to `source/unofficial/current`?

Unofficial development changes are imported explicitly. Ordinary build and
render targets never rewrite `source/unofficial/current` or release-specific
`source/unofficial/edk2-stable*` branches.

The import and render tools also enforce shared source-tree policy checks. In
particular, files under `custom/overlay/` that are byte-identical to their
corresponding `src/` file must be Git symlinks rather than duplicate file
copies. This keeps overlay-only changes visible and prevents accidental drift
from being hidden inside mirrored directories. To run that check explicitly:

```bash
make verify-source-policy
```

When a change is propagated across EDK2 release branches, the import tools also
check whether any changed `custom/overlay/` path refers to a `src/` file that
was renamed or removed in another release branch. By default,
`SOURCE_LIFECYCLE_NORMALISE=exact` automatically applies only deterministic
fix-ups: exact object renames are retargeted, mirror symlinks for source files
that do not exist in an older release branch are dropped, and ambiguous cases
fail before any permanent ref moves. Set `SOURCE_LIFECYCLE_NORMALISE=validate`
to see where a rewrite would be needed without changing scratch trees, `mirror`
to allow only mirror-symlink fix-ups, or `off` to disable this layer.

Use `make import-changes` when the change was made on a materialised
`source/cache/**` branch, on a branch derived from a materialised source tree,
on a legacy source branch, or on any other broader tree. This is the normal
path after following the development example above. The importer finds the
source tree before the intended change, extracts only that diff, applies the
patch to one or more temporary scratch trees, and updates the real source refs
only after every target applies cleanly. A dry run still creates scratch trees
and tries the patch against each target so it can report whether the import is
clean. If the dry run succeeds, those scratch trees are removed. If it
conflicts, the scratch trees are kept under `.cache/edk2-cix/operations/` so
you can resolve and validate the candidate import before any permanent ref or
tag is moved.

For a topic branch created from a persistent materialised cache branch, a
retained source branch, or another unique retained fork point, the patch
extraction base is inferred:

```bash
make import-changes \
  FROM_REF=my-change
```

The reported `BASE_REF` is the tree used to extract the focused patch. It is
not necessarily the destination branch being updated. For example, a topic
branch forked from an older retained source line may correctly use that older
fork point as `BASE_REF` while still applying the resulting small patch to
`source/unofficial/current`. If the dry run reports the expected base, changed
paths, and update target, re-run with `WRITE=1`. The successful dry-run output
prints a ready-to-copy command, followed by the minimum validation command to
run after the ref update. The command includes `BASE_REF` only if you provided
it explicitly:

```bash
make import-changes \
  FROM_REF=my-change \
  WRITE=1
```

For a branch whose base cannot be inferred unambiguously, provide the source
tree before the intended change:

```bash
make import-changes \
  FROM_REF=my-change \
  BASE_REF=<source-tree-before-my-change> \
  WRITE=1
```

After importing to `source/unofficial/current`, build and test the current
source target. When that succeeds, propagate the same change to every
release-specific `source/unofficial/edk2-stable*` branch:

```bash
make propagate-release-branches
```

This target reads the previous `source/unofficial/current` commit from the last
successful import receipt under `.cache/edk2-cix/operations/import-receipts/`
or, if needed, from the local Git reflog. If neither is available, provide the
previous current commit explicitly:

```bash
make propagate-release-branches \
  BASE_REF=<previous-source/unofficial/current-commit>
```

Run the command first as a dry run, then re-run with `WRITE=1` after checking
the output:

```bash
make propagate-release-branches WRITE=1
```

After testing the propagated release branches, move the matching release tags:

```bash
make update-release-tags
make update-release-tags WRITE=1
```

If one target conflicts, no `source/unofficial/**` branch or release tag
is updated. The failed dry run keeps the scratch trees and prints the operation
id, `BASE_REF`, `FROM_REF`, and the scratch path for each target. For generated
operation ids, the importer also creates an ignored root-level shortcut symlink
such as `1778182731` pointing at the scratch tree. Each scratch tree is a
normal detached Git checkout for the target being updated, not the `build`
branch and not `BASE_REF`. Inspect it directly:

```bash
git -C <scratch-tree-or-shortcut> status
git -C <scratch-tree-or-shortcut> diff --name-only --diff-filter=U
git -C <scratch-tree-or-shortcut> diff
```

If Git could not create conflict stages, the importer tries to keep the scratch
tree useful by applying whatever hunks it can and writing `.rej` files for
rejected hunks. In that case, use the `.rej` files and the source diff to apply
the intended change manually:

```bash
git -C /path/to/edk2-cix diff <BASE_REF>..<FROM_REF> -- path/to/file
```

If Git cannot create `.rej` files either, the output prints the extracted patch
stored under `.cache/edk2-cix/operations/.../change.patch`; use that patch as
the manual source of truth.

For conflicts where one side is a symlink and the other is a regular file,
especially under `custom/overlay/`, Git's default conflict view can be
misleading because a symlink side is displayed as only the symlink target
string. The importer writes a symlink-aware conflict report for each
conflicted scratch tree and prints the report path. You can also regenerate
the report:

```bash
make inspect-import-conflicts OP_ID=<operation-id>
```

The report labels `base`, `ours`, and `theirs`, identifies symlink/file
conflicts, expands symlink targets where possible, compares expanded symlink
content with regular-file conflict stages, and writes expanded stage files next
to the report for manual inspection.

For conflicts with Git index stages, the optional resolver can open every
conflicted file in `vimdiff` with symlink stages expanded to their target
content:

```bash
make resolve-conflicts OP_ID=<operation-id>
```

The resolver writes only to the paused scratch tree under `.cache`, stages the
resolved files there, and never moves source refs or tags. If the edited
resolution exactly matches an expanded symlink side, the resolver preserves the
symlink by default instead of accidentally materialising the overlay file. Use
`CONFLICT_EDITOR=<command>` to use a different editor, `CONFLICT_PATHS=...` to
restrict the batch to selected paths, or `PRESERVE_SYMLINKS=0` if you really do
want matching symlink content materialised as a regular file.

If you do not use the resolver, edit the conflicted or rejected files manually,
remove any `.rej` files once they are no longer needed, then stage the resolved
files with `git add` inside that scratch tree. The importer refuses to finalise
while `.rej` files remain, so rejected hunks cannot be silently left behind.
When all printed scratch trees are resolved and staged, validate the candidate
commits without moving refs:

```bash
make import-changes CONTINUE=1 OP_ID=<operation-id>
```

If the operation id starts with a unique numeric prefix, `OP_ID` may use just
that prefix. For example, `OP_ID=1778182731` resolves to
`1778182731-my_topic_branch` when no other paused import
shares the same prefix.

The continue step commits each resolved scratch tree and reports the candidate
commits. If any target is still unresolved, the import remains paused and no
refs move. Once the candidates are ready, deliberately move the requested
`source/unofficial/**` refs in one guarded transaction:

```bash
make import-changes CONTINUE=1 OP_ID=<operation-id> WRITE=1
```

Abort without moving refs:

```bash
make import-changes ABORT=1 OP_ID=<operation-id>
```

If several stale `import-changes` operations are present and all can be
discarded, remove them in one step:

```bash
make import-changes ABORT_ALL=1
```

Use `make import-unofficial-commits` only when `FROM_REF` is already a topic
branch based directly on the `source/unofficial/**` branch being updated. This
tool advances or replays commits that were developed on an unofficial source
branch; it deliberately rejects generated `source/cache/**` refs and topics
based on materialised cache refs.

Dry run an already-unofficial-based topic first:

```bash
make import-unofficial-commits \
  FROM_REF=my-change
```

Then update the unofficial source branch deliberately:

```bash
make import-unofficial-commits \
  FROM_REF=my-change \
  WRITE=1
```

To apply the same finished topic branch to every release-specific branch,
ask the importer to propagate it:

```bash
make import-unofficial-commits \
  FROM_REF=my-change \
  PROPAGATE_RELEASE_BRANCHES=all
```

If the dry run reports the expected replay range and release-branch list, re-run
with `WRITE=1`:

```bash
make import-unofficial-commits \
  FROM_REF=my-change \
  PROPAGATE_RELEASE_BRANCHES=all \
  WRITE=1
```

Propagation prepares all candidate release-branch commits under
`.cache/edk2-cix/operations/import-unofficial/` before moving any permanent
refs. If one release branch conflicts, no `source/unofficial/**` branch is
updated. Resolve the conflicts in the scratch tree printed by the command, then
continue:

```bash
make import-unofficial-commits CONTINUE=1 OP_ID=<operation-id> WRITE=1
```

Or abort without moving refs:

```bash
make import-unofficial-commits ABORT=1 OP_ID=<operation-id>
```

The key variables are:

- `FROM_REF` is the topic branch or commit containing your finished change.
- `BASE_REF` is the source tree before the intended change.
  `make import-changes` infers this for topics based on a unique
  `source/cache/**` branch, `source/unofficial/current`, or retained branch
  fork point. Pass it explicitly only when the importer reports ambiguity or
  cannot find the intended base.
- `SOURCE_UNOFFICIAL_REF` is the full unofficial source branch to update. It
  defaults to `source/unofficial/current` and must stay under
  `source/unofficial/`.
- `PROPAGATE_RELEASE_BRANCHES=all` replays or applies the imported change onto
  every `source/unofficial/edk2-stable*` release branch.
- `UPDATE_RELEASE_TAGS=1` may be used with
  `PROPAGATE_RELEASE_BRANCHES=all` to move the matching
  `source/unofficial/edk2/stable-*` tags only after every replay has
  succeeded. The safer staged workflow is to omit it, test the propagated
  release branches, then run `make update-release-tags` separately.
- `SOURCE_LIFECYCLE_NORMALISE=off|validate|mirror|exact` controls deterministic
  overlay path normalisation while propagating changes across EDK2 release
  branches. The default is `exact`.
- `COMMIT_MESSAGE` sets the commit message used by `make import-changes`.
  Literal `\n` sequences are treated as separate `git commit -m` paragraphs.
- `COMMIT_MESSAGE_FILE` reads the `make import-changes` commit message from a
  file and is mutually exclusive with `COMMIT_MESSAGE`.
- `SIGNOFF=1` adds a `Signed-off-by` trailer with `git commit -s`.
- If neither `COMMIT_MESSAGE` nor `COMMIT_MESSAGE_FILE` is set, the importer
  inherits the full commit message from the `FROM_REF` tip commit.

For each supported EDK2 release, the repo keeps two related records of the
unofficial project changes:

- an unofficial source branch, such as
  `source/unofficial/edk2-stable202602`, which is a normal source branch known
  to apply cleanly to that EDK2 release
- an unofficial release tag, such as `source/unofficial/edk2/stable-202602`,
  which marks the commit known to apply to that EDK2 release without colliding
  with the branch name

Here, an EDK2 release means the upstream EDK2 code together with its matching
`edk2-platforms` and `edk2-non-osi` companion sources. Some unofficial changes
apply unchanged across several EDK2 releases; others need small adjustments
because upstream files moved or changed.

For example, if the same unofficial source commit works on both
`edk2-stable202502` and `edk2-stable202505`, both release tags may point at
that commit. If `edk2-stable202508` needs an extra adjustment, the importer
pauses at that release branch and lets you resolve the release-specific conflict
before it updates any branch or tag.

Release tags must remain reachable from retained `source/unofficial/**`
branches, rather than becoming tag-only orphan commits. The tags deliberately
use the non-colliding `source/unofficial/edk2/stable-*` namespace while the
matching branches use `source/unofficial/edk2-stable*`.

`FROM_REF=source/unofficial/current` and
`FROM_REF=source/unofficial/edk2-stable*` are rejected for propagation by
default. Use a normal topic branch as the input. Maintainers can use
`ALLOW_SOURCE_REF_FROM=1` with an explicit `BASE_REF` for recovery workflows,
but ordinary development should not need this escape hatch.

## How do I update upstream EDK2, Arm TF-A, OP-TEE, CIX, or Radxa sources?

Use `make integrate-source-release`. Without `WRITE=1`, it validates arguments
and prints the refs it would create.

Examples:

```bash
make integrate-source-release TYPE=upstream COMPONENT=edk2 RELEASE=edk2-stable202602
make integrate-source-release TYPE=upstream COMPONENT=tf-a RELEASE=v2.7
make integrate-source-release TYPE=vendor VENDOR=cix RELEASE=1.2
make integrate-source-release TYPE=vendor VENDOR=radxa RELEASE=1.2.1 EDK2_BASE=edk2-stable202208 REF=<vendor-ref>
make integrate-source-release TYPE=vendor VENDOR=radxa RELEASE=1.2.1 EDK2_BASE=edk2-stable202602 RADXA_SOURCE=port REF=<ported-ref>
```

When the dry run is correct, add `WRITE=1`. The same change updates
`config/refs-*.json` with the new object IDs and tree IDs before being
committed.

`TYPE=upstream` is for base components: `edk2`, `edk2-platforms`,
`edk2-non-osi`, `tf-a`, or `op-tee`. `TYPE=vendor` is for a vendor integration
target. `VENDOR=radxa` records an actual Radxa-published source tree under
`source/vendor/radxa/**` or this project's port of that source tree under
`source/port/radxa/**`. `VENDOR=cix` updates the CIX release bundle, whose TF-A
and OP-TEE contents are tracked as separate internal components so future
Arm-upstream uplifts remain possible.

The CIX bundle records original vendor provenance. To start an experimental
uplift of one CIX component to a newer Arm upstream:

1. Integrate the Arm base you want to try:

   ```bash
   make integrate-source-release TYPE=upstream COMPONENT=tf-a RELEASE=v2.12 WRITE=1
   make integrate-source-release TYPE=upstream COMPONENT=op-tee RELEASE=4.4.0 WRITE=1
   ```

2. Create a normal topic branch from the selected Arm base and port the CIX
   component changes onto it.
3. When the port builds and has been reviewed, record the resulting component
   ref:

   ```bash
   make integrate-source-release \
     TYPE=vendor VENDOR=cix RELEASE=1.2 \
     COMPONENT=tf-a ARM_BASE=v2.12 \
     REF=<ported-tf-a-ref> WRITE=1
   ```

   The recorded ref is `source/component/cix/1.2/tf-a/v2.12`. Use
   `COMPONENT=op-tee ARM_BASE=4.4.0` for OP-TEE.

4. Record any additional source refs or metadata needed for the derived
   source-target list to select the uplifted component ref, render the
   resulting source target, and run the same build/audit qualification used for
   EDK2 releases.

The component port itself is intentionally a source-level engineering step
rather than an automatic patch replay. The deterministic part starts once the
reviewed component ref is recorded in `source/component/cix/**` and
`config/refs-cix.json`.

Radxa non-release updates use the same vendor integration path. First update or
fetch the vendor source branch into a local ref, then give the source
release branch a release-like name that records the most recent release plus the
vendor commit, for example:

```bash
make integrate-source-release \
  TYPE=vendor VENDOR=radxa \
  RELEASE=1.2.1+<short-commit> \
  EDK2_BASE=edk2-stable202208 \
  REF=main
```

For Radxa vendor refs, `MATERIALISE=1` is the default. This recursively
flattens a submodule-shaped vendor source ref before the
`source/vendor/radxa/**` or `source/port/radxa/**` source ref is recorded, so
the retained source tree contains ordinary file content rather than gitlinks.

Vendor CI workflow changes are handled separately from firmware source changes.
The build branch has its own GitHub Actions workflows, so upstream Radxa
`.github/workflows` files are not executed directly here. Run this check after
integrating any new Radxa vendor source ref:

```bash
make check-vendor-workflow-drift
```

If it fails, inspect the changed vendor workflow files and port any relevant
intent to `.github/workflows/` on this branch before updating
`config/vendor-workflow-baseline.json`. This keeps vendor CI changes visible
without inheriting vendor workflows that assume the old submodule-based `main`
branch layout.

After adding source refs for a new supported EDK2 release:

1. Integrate the required source refs with `make integrate-source-release`.

   At minimum this means the upstream `edk2` base ref and the selected
   companion `edk2-platforms` and `edk2-non-osi` refs. Once
   `source/base/edk2/<edk2-release>` exists, the release is discoverable; there
   is no separate release-list file to update.

2. Generate the source target once and refresh its recorded tree ID:

   ```bash
   make render-release-branch \
     RELEASE=edk2-202605/cix-1.2/radxa-1.2.1/unofficial \
     PERSIST=1 REBUILD=1 FORCE=1
   ```

3. Run `make verify-build-matrix` to confirm the derived matrix, source-target
   manifest, refs, aliases, and tree IDs agree.
4. Run the normal build/audit qualification for the new source target before
   publishing it.

The persistent `source/cache/release/**` branch created in step 2 is a cache.
Once the tree ID has been recorded and validation passes, it may be deleted
without losing the ability to regenerate the source target.

`make help-source-targets` lists source targets derived from the available
EDK2, Radxa, CIX, and unofficial refs.

To create a smaller bare repository containing only the build branch plus the
non-cache source refs and tags required for deterministic reconstruction, use:

```bash
make create-minimised-clone DIR=/path/to/edk2-cix.minimal.git
```

The destination directory must be empty or absent. Generated `source/cache/**`,
legacy, private, and diagnostic branches are omitted from the export. The
exported repository's default branch/`HEAD` is set to `build`, so a normal
clone checks out the firmware build interface rather than an unborn or legacy
branch.

To prove that a minimised export can be cloned normally, validated, and used to
render the default source target, run:

```bash
make verify-minimised-clone
```

This creates a temporary verification workspace under `.cache/edk2-cix/tmp` and
removes it when the check completes. Set `KEEP=1` to retain that workspace for
inspection, or `DIR=<path>` to choose an explicit workspace directory.

## How do I project `source/unofficial/current` to a materialised firmware branch?

Render a configured source target that includes the unofficial layer:

```bash
make render-release-branch \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/unofficial \
  PERSIST=1
```

Then validate it:

```bash
make verify-release-branch \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/unofficial
```

Materialised refs are generated mechanically from `source/base/**`,
`source/vendor/**`, `source/port/**`, `source/component/**`, and
`source/unofficial/**` release branches. They may be stored as
`source/cache/release/**` branches for convenience, but ordinary build and
validation targets regenerate them when those branches are absent.

## How do I test a floating upstream tip instead of the latest release?

Integrate the upstream component using an explicit `REF` or a release-like name
chosen for the experiment, then render a test source-target branch that
references it.

```bash
make integrate-source-release TYPE=upstream COMPONENT=edk2 REF=<upstream-object> RELEASE=floating-test WRITE=1
```

Floating-tip tests should use clearly named experimental refs and should not
replace stable `source/base/**` refs until the release mapping and validation
results are recorded in `config/`.

## How do I work when an upstream remote is unavailable?

Ordinary builds do not require GitHub or any external upstream/vendor remote
when the required source refs and manifests are present in this repository
clone.

The fallback order is:

1. Use a generated `source/cache/release/**` cache if it exists.
2. Regenerate the selected source target from source refs and manifests.
3. For source integration only, contact the external upstream/vendor remote if
   required objects are missing.
4. Fail only if required source data is unavailable locally and remotely.

There is no separate offline mode flag. The scripts try to proceed from local
data first and warn or fail only when missing objects make the requested
operation impossible.

## How does CI work?

The build branch carries its own workflows under `.github/workflows/`:

- `Build branch CI` fetches the required `source/**` refs, runs `make test`,
  and runs `make lint`.
- `Firmware build` is a manual workflow for rendering and building one selected
  source target, or for running `build-all`.
- `Deterministic replay` renders the replay-capable EDK2 `202208` unofficial
  source target, downloads the latest Radxa release package, and checks that
  the upstream-path rebuild still matches the published payload for O6 and O6N.
- `Secure Boot audit` renders the selected custom source target, checks the
  pinned Microsoft Secure Boot payload metadata and release version, then
  builds and validates the embedded Secure Boot defaults for O6 and O6N.
- `Upstream versions` checks whether recorded EDK2, Radxa, CIX, Arm TF-A,
  OP-TEE, and Microsoft Secure Boot source inputs lag their external remotes.
- `Build documentation` builds the mdBook product documentation from `docs/`
  and uploads the GitHub Pages artefact.

The lint suite checks JSON, YAML, Markdown, shell scripts, and Python scripts
in the build branch using the same `make lint` target available locally.
Dependabot is configured for GitHub Actions updates on the repository default
branch. If a minimised clone is pushed as its own repository, that default
branch should be `build`.

The build branch is the CI control plane. Workflows may render and test a
selected firmware tree, but the workflow logic itself lives here rather than
inside `source/unofficial/**`. Keeping CI here makes it visible from the
default branch, avoids hidden behaviour in rendered source trees, and keeps
vendor workflow changes reviewable as source inputs rather than executable
policy.

The inherited Radxa `main` workflows are retained as vendor source data under
`source/vendor/radxa/**`, but they are not run by this branch. Vendor
packaging, release, and linked-issue workflows assume Radxa's upstream
repository process and are not useful for normal firmware builds here.
`make check-vendor-workflow-drift` detects changes to those vendor workflow
files so the build branch workflows can be reviewed and updated deliberately.

Product documentation is kept under `docs/` so documentation-specific
`book.toml`, `theme/`, `devenv*`, and helper scripts do not clutter the
build-branch root. Build it with:

```bash
make docs-build
```

`make docs-build` defaults to `DOCS_BUILD_MODE=auto`. It uses the host
`devenv`/`cargo` toolchain when those tools are available, and otherwise falls
back to the same documentation container used by `make docs-workflow-local`.
Use `DOCS_BUILD_MODE=host` to require a host-only build or
`DOCS_BUILD_MODE=container` to force the container path.

To check upstream versions locally:

```bash
make check-upstream-versions
make check-upstream-versions UPSTREAM_VERSION_MODE=advisory
make check-upstream-versions UPSTREAM_VERSION_ONLY=edk2,radxa
make check-upstream-versions UPSTREAM_VERSION_ONLY=edk2:release
```

Each source reports release freshness separately from branch-head commit drift.
Most sources use tags for releases; CIX sources that publish release-labelled
commits instead are matched by commit subject. Release drift is the important
signal for most sources. Branch-head drift is usually advisory because
unreleased commits may be noisy or transient. The scheduled GitHub Actions
workflow runs in `policy` mode and publishes a summary table. In that mode,
stale checks marked `strict` in `config/upstream-versions.json` fail the
workflow, while advisory checks report warnings without failing. Use
`UPSTREAM_VERSION_MODE=strict` when you want any stale source to fail.

To try GitHub Actions locally with `act`:

```bash
make gha-act-list
make gha-act-dry-run ACT_WORKFLOW=.github/workflows/upstream-versions.yaml
make gha-act-run ACT_WORKFLOW=.github/workflows/upstream-versions.yaml ACT_JOB=upstream-versions
```

The wrapper downloads a pinned `act` binary into `.cache/edk2-cix/tools/act/`,
verifies the release checksum, and stores `act` cache data under
`.cache/edk2-cix/act-cache/`.

## Validation checklist

For normal firmware building, the build target itself performs the necessary
preflight checks. When changing source refs, source-target manifests, or
materialised branches, run:

```bash
make test
make lint
make verify-build-matrix
make verify-manifest-integrity
make verify-minimised-clone
make check-identity-integrity
make verify-identity-integrity
make check-ref-integrity
make check-help-cache
make check-vendor-workflow-drift
make ref-report
```

For a materialised branch, also run:

```bash
make verify-release-branch RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/unofficial
```
