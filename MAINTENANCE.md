# Repository Maintenance

This document is intentionally maintained on the `test` branch. The default `build` branch README remains focused on end-user firmware usage.

1. Choose the source target you want to develop against by running
   `make help-source-targets`.
2. Materialise that source target.
3. Create a normal topic branch from the materialised firmware branch.
4. Build and test your change.
5. Import the finished change to the intended
   `source/unofficial/<line>/current` branch with `make import-changes`.

Example:

```bash
make render-release-branch \
  RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial \
  PERSIST=1
git switch -c my-change source/cache/release/custom/edk2-202608/cix-1.2/radxa-1.3.1/unofficial
```

The supported workflows guard `source/base/**`, `source/vendor/**`, and
`source/port/**` refs by comparing them against the expected object IDs and
tree IDs in `config/refs-*.json`. Git itself does
not make those branch names immutable, so do not edit or move them by hand. If
a guarded ref moved unexpectedly, or is checked out in a dirty worktree, the
scripts abort before using it. Only `make integrate-source-release` should
create or advance those refs.

## How do I persist development changes to an Unofficial line?

Unofficial development changes are imported explicitly. Ordinary build and
render targets never rewrite mutable `source/unofficial/<line>/current` refs,
immutable release checkpoints, or historical `source/unofficial/edk2-stable*`
compatibility branches. Import targets default to the line selected in
`config/policies.json`; set `SOURCE_UNOFFICIAL_REF` when updating another line:

```bash
make import-changes \
  FROM_REF=my-change \
  SOURCE_UNOFFICIAL_REF=source/unofficial/1.3/current
```

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
conflicts, the scratch trees are kept under
`.worktrees/edk2-cix-tmp/operations/` at the shared repository root so
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
the policy-selected line tip. If the dry run reports the expected base, changed
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

After importing to the selected line tip, build and test that line's source
target. If the same focused project change should also apply to every retained
historical EDK2 compatibility branch, propagate it explicitly:

```bash
make propagate-release-branches
```

This target reads the previous selected line-tip commit from the last successful
import receipt under
`.worktrees/edk2-cix-tmp/operations/import-receipts/` or, if
needed, from that ref's local Git reflog. If neither is available, provide the
previous line-tip commit explicitly:

```bash
make propagate-release-branches \
  BASE_REF=<previous-line-tip-commit>
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
stored under `.worktrees/edk2-cix-tmp/operations/.../change.patch`; use that
patch as
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
`.worktrees/edk2-cix-tmp/operations/import-unofficial/` before moving any permanent
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
  `source/cache/**` branch, selected `source/unofficial/<line>/current`, or
  retained branch fork point. Pass it explicitly only when the importer reports
  ambiguity or cannot find the intended base.
- `SOURCE_UNOFFICIAL_REF` is the full unofficial source branch to update. It
  defaults to the policy-selected `source/unofficial/<line>/current` ref and
  must stay under `source/unofficial/`.
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
  `source/unofficial/edk2-stable202608`, which is a normal source branch known
  to apply cleanly to that EDK2 release
- an unofficial release tag, such as `source/unofficial/edk2/stable-202608`,
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

`FROM_REF=source/unofficial/<line>/current` and
`FROM_REF=source/unofficial/edk2-stable*` are rejected for propagation by
default. Use a normal topic branch as the input. Maintainers can use
`ALLOW_SOURCE_REF_FROM=1` with an explicit `BASE_REF` for recovery workflows,
but ordinary development should not need this escape hatch.

## How do I update upstream EDK2, Arm TF-A, OP-TEE, CIX, or Radxa sources?

Use `make integrate-source-release`. Without `WRITE=1`, it validates arguments
and prints the refs it would create or reports that the requested immutable ref
is already integrated.

Examples:

```bash
make integrate-source-release TYPE=upstream COMPONENT=edk2 RELEASE=edk2-stable202608
make integrate-source-release TYPE=upstream COMPONENT=tf-a RELEASE=v2.7
make integrate-source-release TYPE=vendor VENDOR=cix RELEASE=1.2
make integrate-source-release TYPE=vendor VENDOR=radxa RELEASE=1.2.1 EDK2_BASE=edk2-stable202208 REF=<vendor-ref>
make integrate-source-release TYPE=vendor VENDOR=radxa RELEASE=1.3.1 EDK2_BASE=edk2-stable202608 RADXA_SOURCE=port REF=<ported-ref>
```

When the dry run is correct, add `WRITE=1`. The same change updates
`config/refs-*.json` with the new object IDs and tree IDs before being
committed.

Published `source/**` branches may be present in a clone only as
`origin/source/**` remote-tracking refs. Plain `git branch` lists local branches
only; use `git branch -r` or `git branch -a` to inspect those refs. Repeating an
integration whose target and recorded upstream provenance already match is an
idempotent no-op in both dry-run and `WRITE=1` modes.

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

   The recorded ref is `source/port/cix/1.2/tf-a/v2.12`. Use
   `COMPONENT=op-tee ARM_BASE=4.4.0` for OP-TEE.

4. Record any additional source refs or metadata needed for the derived
   source-target list to select the uplifted component ref, render the
   resulting source target, and run the same build/audit qualification used for
   EDK2 releases.

The component port itself is intentionally a source-level engineering step
rather than an automatic patch replay. The deterministic part starts once the
reviewed component ref is recorded in `source/port/cix/**` and
`config/refs-cix.json`.

Radxa non-release updates use the same vendor integration path. First update or
fetch the vendor source branch into a local ref, then give the source
release branch a release-like name that records the most recent release plus the
vendor commit, for example:

```bash
make integrate-source-release \
  TYPE=vendor VENDOR=radxa \
  RELEASE=<current-release>+<short-commit> \
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
`config/vendor-workflow-baseline.json`. Preview the mechanical metadata update
with:

```bash
make refresh-vendor-workflow-baseline
```

After the review and any required CI port, record that decision with:

```bash
make refresh-vendor-workflow-baseline REVIEWED=1 WRITE=1
```

The refresh groups releases carrying byte-identical workflow snapshots and
creates a new baseline only when workflow content changed. This keeps vendor CI
changes visible without inheriting vendor workflows that assume the old
submodule-based `main` branch layout.

### Radxa firmware release uplift

Every Radxa release, including the release currently used by a development
line, has its own immutable raw vendor ref. There is no special case that hides
or deletes the `1.2.1` source. From a `build` branch worktree, fetch and record
the new vendor tag first:

```bash
git fetch --no-tags \
  https://github.com/radxa-pkg/edk2-cix.git \
  refs/tags/<new-release>

make integrate-source-release \
  TYPE=vendor VENDOR=radxa \
  RELEASE=<new-release> \
  EDK2_BASE=edk2-stable202208 \
  RADXA_SOURCE=vendor \
  REF=FETCH_HEAD

make integrate-source-release \
  TYPE=vendor VENDOR=radxa \
  RELEASE=<new-release> \
  EDK2_BASE=edk2-stable202208 \
  RADXA_SOURCE=vendor \
  REF=FETCH_HEAD \
  WRITE=1
```

The integration materialises submodules, creates
`source/vendor/radxa/<new-release>/edk2-stable202208`, and records the fetched
tag commit as `upstream_ref` in `config/refs-radxa.json`. Existing matching
releases are idempotent. An existing immutable ref with different contents or
provenance is rejected rather than silently replaced.

Next carry one maintained Unofficial line across the adjacent Radxa delta. Run
the complete command as a dry run first:

```bash
make uplift-radxa-release \
  FROM_RELEASE=<previous-release> \
  TO_RELEASE=<new-release> \
  LINE=<major.minor> \
  EDK2_BASE=edk2-stable202608 \
  CIX_RELEASE=1.2
```

When the plan is correct, add `WRITE=1`. The target:

1. replays only the adjacent raw vendor delta onto the previous EDK2 port
2. records `source/port/radxa/<new-release>/<edk2-base>`
3. carries the previous Unofficial delta onto that port
4. records immutable
   `source/unofficial/<new-release>/<edk2-base>` provenance
5. advances only `source/unofficial/<major.minor>/current`
6. updates that line's policy, renders its source target, and verifies the
   complete source-target matrix

For an ordinary same-line uplift, the source is the exact checkpoint for
`FROM_RELEASE`. The retained `1.3` line was historically initialised from the
reviewed `1.2` tip with this deliberate override:

```bash
make uplift-radxa-release \
  FROM_RELEASE=1.2.4 \
  TO_RELEASE=1.3.0 \
  LINE=1.3 \
  FROM_UNOFFICIAL_REF=source/unofficial/1.2/current \
  EDK2_BASE=edk2-stable202605 \
  CIX_RELEASE=1.2 \
  WRITE=1
```

Subsequent `1.3.x` uplifts use the previous exact `1.3.x` checkpoint
automatically. `MAKE_DEFAULT=1` may select the updated line immediately, but
the safer staged workflow is to qualify both boards first and then run:

```bash
make select-unofficial-line LINE=<major.minor>
make select-unofficial-line LINE=<major.minor> WRITE=1
```

Mechanical source and overlay changes are automated. If either port contains a
genuine source conflict, no permanent target ref moves. The tool preserves a
worktree, identifies whether the conflict is in the source or overlay stage,
and prints the resume command. Resolve and commit the reviewed choice there,
then rerun with `PORT_REF=<commit>` or `UNOFFICIAL_REF=<commit>`. Normally the
resume stage is recovered from the commit trailer; use
`UNOFFICIAL_REF_STAGE=source|overlay|final` only when deliberately overriding
that detection.

Completed stages are idempotent. `SKIP_RENDER=1` and `VERIFY=0` are recovery or
diagnostic controls, not the normal release path. `ALLOW_REPLACE=1` is reserved
for a reviewed correction to an already recorded immutable ref.

Qualify the resulting line with both boards and both custom-fix states before
selecting it as the default:

```bash
make buildbox-firmware-build \
  RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial-1.3.1 \
  FIRMWARE_BOARD=O6 ENABLE_FIRMWARE_FIXES=true
make buildbox-firmware-build \
  RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial-1.3.1 \
  FIRMWARE_BOARD=O6N ENABLE_FIRMWARE_FIXES=true
make buildbox-firmware-build \
  RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial-1.3.1 \
  FIRMWARE_BOARD=O6 ENABLE_FIRMWARE_FIXES=false
make buildbox-firmware-build \
  RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial-1.3.1 \
  FIRMWARE_BOARD=O6N ENABLE_FIRMWARE_FIXES=false
```

### Upstream EDK2 stable release uplift

For a new upstream EDK2 stable release, prefer the orchestrated target. One
invocation advances exactly one maintained Unofficial line; `LINE` defaults to
the policy-selected line and all line-specific defaults are read from that
line's policy record:

```bash
make uplift-edk2-release \
  EDK2_BASE=edk2-stable202608 \
  FROM_EDK2_BASE=edk2-stable202605 \
  LINE=1.3
```

Run it once without `WRITE=1` first. The dry run validates the selected
release, shows which stages are already complete, and delegates dry runs for
stages whose prerequisites already exist. Later stages that depend on newly
recorded refs are reported as pending until the real run creates those refs.
When the plan is correct, run:

```bash
make uplift-edk2-release \
  EDK2_BASE=edk2-stable202608 \
  FROM_EDK2_BASE=edk2-stable202605 \
  LINE=1.3 \
  WRITE=1
```

`FROM_EDK2_BASE` defaults to
`config/policies.json` `unofficial_source_policy.current_edk2_release`, so it
can be omitted for a first run before policy has moved. Passing it explicitly
is safer in notes, scripts, and reruns. Keep `LINE` explicit as well when more
than one line is maintained.

Repeat the dry-run/review/write sequence for every actively maintained line.
The current policy maintains only line `1.3`/Radxa `1.3.1`; line `1.2`/Radxa
`1.2.4` remains available through its retained immutable refs but is not moved
by routine uplifts.

The target performs the mechanical stages in order:

1. records `source/base/edk2/<EDK2_BASE>`
2. records `source/base/edk2-platforms/<EDK2_BASE>`
3. records `source/base/edk2-non-osi/<EDK2_BASE>`
4. ports the retained EDK2 compatibility source and records both
   `source/unofficial/<EDK2_BASE>` and its
   `source/unofficial/edk2/stable-*` tag
5. ports the selected line's Radxa source release to
   `source/port/radxa/**/<EDK2_BASE>`
6. ports the selected `source/unofficial/<line>/current` tree and records the
   exact `source/unofficial/<radxa-release>/<EDK2_BASE>` checkpoint
7. moves only that line's mutable current ref and updates `config/policies.json`
8. refreshes the rendered `source/cache/release/custom/.../unofficial`
   branch for the selected line's source target
9. runs `make verify-build-matrix`

By default the Radxa release and CIX release are taken from
`config/policies.json`. Override them when carrying forward a different vendor
input:

```bash
make uplift-edk2-release \
  EDK2_BASE=edk2-stable202608 \
  FROM_EDK2_BASE=edk2-stable202605 \
  RADXA_RELEASE=1.3.1 \
  CIX_RELEASE=1.2 \
  WRITE=1
```

For `edk2-platforms` and `edk2-non-osi`, omit explicit refs for normal stable
EDK2 releases. The integration helper selects the latest upstream `master`
commit at or before the matching EDK2 stable tag timestamp and records that
timestamp in `config/refs-edk2.json`. If a future release needs a different
selection, pass `EDK2_PLATFORMS_REF=<ref>` or `EDK2_NON_OSI_REF=<ref>` and
record the reason in the commit message.

If a source port stops with a conflict, that is a real engineering decision
rather than a script special case. The script prints the preserved worktree
path, writes a `README.md` beside it, and stops before moving the target ref.
Resolve the conflict markers, commit the resolved tree in that worktree, then
rerun the orchestrated target with the appropriate resolved ref:

```bash
# If the retained EDK2 compatibility stage stopped:
make uplift-edk2-release \
  EDK2_BASE=edk2-stable202608 \
  FROM_EDK2_BASE=edk2-stable202605 \
  LINE=1.3 \
  COMPATIBILITY_REF=<resolved-compatibility-commit> \
  WRITE=1

# If the Radxa stage stopped:
make uplift-edk2-release \
  EDK2_BASE=edk2-stable202608 \
  FROM_EDK2_BASE=edk2-stable202605 \
  LINE=1.3 \
  RADXA_REF=<resolved-radxa-commit> \
  WRITE=1

# If the unofficial stage stopped:
make uplift-edk2-release \
  EDK2_BASE=edk2-stable202608 \
  FROM_EDK2_BASE=edk2-stable202605 \
  LINE=1.3 \
  UNOFFICIAL_REF=<resolved-unofficial-commit> \
  WRITE=1
```

Completed stages are skipped on rerun. If policy already moved before a rerun,
keep passing the previous base explicitly with `FROM_EDK2_BASE=...`. Use
`ALLOW_REPLACE=1` only when deliberately replacing a ref that was already
recorded.

The primitive targets remain supported and are what the orchestrated target
uses internally:

- `make integrate-source-release` records upstream, vendor, and CIX component
  refs. It is still the right tool for individual source refs and non-EDK2
  component experiments.
- `make promote-unofficial-compatibility` ports the retained EDK2 compatibility
  source and records its matching compatibility branch and tag.
- `make promote-unofficial-release` promotes one unofficial source tree to one
  new EDK2 base. Use it directly when you need to inspect or test that stage in
  isolation.
- `make render-release-branch` materialises one configured source target and
  refreshes the generated tree-ID manifest.
- `make extract-vendor-delta` remains a read-only inspection/reporting helper.
- `make import-changes`, `make import-unofficial-commits`,
  `make propagate-release-branches`, `make inspect-import-conflicts`, and
  `make resolve-conflicts` remain for project-source changes across existing
  Unofficial line tips and retained historical EDK2 compatibility branches.
  They are not firmware-release or EDK2-base uplift targets.

After the orchestrated uplift has completed, qualify the result before
publishing:

```bash
make test-local
make deterministic-replay \
  REPLAY_VERSION=1.3.1 \
  REPLAY_INPUT=/path/to/edk2-cix_1.3.1_all.deb
```

`make test-local` includes matrix, metadata, lifecycle, minimised-clone, help,
workflow-drift, and identity checks. The deterministic replay intentionally
uses the pinned `edk2-202208` vendor source target; it proves the enhanced
uplift tooling did not drift the byte-identical upstream replay path while the
new current target moved forward.

Publish the resulting build metadata commit and its source refs atomically.
The target is a dry run unless `WRITE=1` is explicit:

```bash
make publish-source-update \
  SOURCE_REFS=source/base/edk2/edk2-stable202608
make publish-source-update \
  SOURCE_REFS=source/base/edk2/edk2-stable202608 \
  WRITE=1
```

Comma-separate every source ref recorded by the metadata commit. The publisher
requires a clean `build` checkout that is a fast-forward of the remote branch,
checks each recorded object and tree ID, refuses replacement of an immutable
remote source ref, and sends `build` plus all selected source refs in one
atomic push. Mutable Unofficial refs use exact force-with-lease expectations.

`test` is deliberately not part of that atomic source-data transaction. After
the source update is committed to `build`, fast-forward or rebase `test` to the
same build commit, update any test-only coverage, validate it, and publish that
branch separately.

The persistent `source/cache/release/**` branch created by the render stage is a cache.
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

This creates a temporary verification workspace under the shared repository's
`.worktrees/edk2-cix-tmp` directory and removes it when the check completes.
Set `KEEP=1` to retain that workspace for inspection, or `DIR=<path>` to choose
an explicit workspace directory. Set `EDK2_CIX_TMP_ROOT=<path>` when all source
porting and verification scratch for a run should share another explicit root.
Delegated command output is streamed to the terminal and therefore to the
GitHub Actions log. While a delegated command remains active, the verifier also
prints a heartbeat every 30 seconds. `V=1` additionally prints each delegated
command line.

The cloned export runs the same publication quality gates as `make test`,
including unit, source-policy, source-lifecycle, metadata, vendor-workflow, and
identity checks. Only the nested minimised-export invocation is skipped to
avoid recursion.

## How do I materialise an Unofficial line?

Render a configured source target that includes the unofficial layer:

```bash
make render-release-branch \
  RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial \
  PERSIST=1
```

Then validate it:

```bash
make verify-release-branch \
  RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial
```

Materialised refs are generated mechanically from `source/base/**`,
`source/vendor/**`, `source/port/**`, and `source/unofficial/**` release
branches. They may be stored as
`source/cache/release/**` branches for convenience, but ordinary build and
validation targets regenerate them when those branches are absent.

Unofficial checkpoints retain the project source carried across releases. At
render time, `VERSION` and `debian/changelog` are aligned with the immutable
`source/vendor/radxa/<release>/edk2-stable202208` release source. This prevents
an older checkpoint's package identity from being mislabeled as the selected
Radxa release while preserving the checkpoint and its provenance unchanged.

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

The default `build` branch is the GitHub Actions control plane. GitHub discovers
scheduled and manually dispatched workflows from the default branch, so these
minimal entry points cannot live only on `test`:

- `Firmware qualification` runs on every build push, pull request, merge-queue
  candidate, and manual dispatch. Firmware-affecting candidates run source
  tests and lint, the O6/O6N current-source matrix with fixes both off and on,
  and the O6/O6N exact Radxa 1.3.1 replay. Documentation/licensing-only changes
  stop after classification.
- `Manual firmware build` renders and builds one selected current source target
  using explicit workflow inputs.
- `Current-source firmware validation` provides the reusable and manually
  dispatched O6/O6N, fixes-off/on Trixie matrix.
- `Deterministic replay` provides the reusable and manually dispatched O6/O6N
  replay of Radxa 1.3.1 from its historical EDK2 `202208` base.
- `Upstream versions` is the only scheduled Actions workflow. It also runs when
  relevant configuration or tooling changes.
- `Build documentation` builds the mdBook site when its inputs change. Only a
  push to `build` deploys Pages; pull requests, merge-queue candidates, calls
  from `test`, and local `act` runs build without deploying.

The `test` branch contains this maintainer documentation, the extended
regression suite, and `test-branch-ci.yaml`. Keep it based on the exact `build`
candidate under review. The test workflow runs on test-branch pushes and pull
requests targeting `test`, and it calls the same reusable documentation
workflow retained on `build`.

The stable `Qualification summary` job is the branch-protection target. It
reports every gate and fails when any required result fails. Superseded pull
request runs are cancelled; published pushes, manual runs, current-source
matrices, and exact replays are allowed to finish.

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

Every configured source remote must have a freshness check or an explicit
`monitor: false` record with a reason. This includes EDK2 and its Platforms and
Non-OSI companions, Radxa, CIX BIOS, bootloader1, TF-A and OP-TEE forks, Arm
TF-A and OP-TEE, and Microsoft Secure Boot payloads. Tooling checks cover the
pinned Actions, `act`, runner image, Nix, and documentation tools.

Each source reports release freshness separately from branch-head commit drift.
Most sources use tags for releases; CIX sources that publish release-labelled
commits instead are matched by commit subject. EDK2 Platforms and Non-OSI use a
strict date-coupled rule: their selected commit must be the latest upstream
commit at or before the matching EDK2 stable-tag timestamp. Release drift and
immutable/date-coupled selection drift are strict. Floating development heads
are advisory because unreleased commits may be noisy or transient.

The Arm TF-A and OP-TEE releases embedded in the CIX bundle are intentional
vendor baselines rather than freely replaceable tool pins. Their newer upstream
releases remain visible as advisories until a separately qualified CIX port is
available.

The same checker also reports advisory freshness for local build infrastructure
pins such as the repo-managed `act` version, pinned GitHub Actions major
versions, the tagged local `act` runner container image, and the documentation
workflow's Nix base image. The scheduled GitHub Actions workflow
runs in `policy` mode and publishes a summary table. In that mode, stale
checks marked `strict` in `config/upstream-versions.json` fail the workflow,
while advisory checks report warnings without failing. Use
`UPSTREAM_VERSION_MODE=strict` when you want stale non-advisory source or
tooling pins to fail while advisory branch-head drift, such as unreleased
commits on an upstream `main` branch, still reports without failing.

To try GitHub Actions locally with `act`:

```bash
make gha-act-list
make gha-act-dry-run ACT_WORKFLOW=.github/workflows/upstream-versions.yaml
make gha-act-run ACT_WORKFLOW=.github/workflows/upstream-versions.yaml ACT_JOB=upstream-versions
```

The wrapper downloads a pinned `act` binary into `.cache/edk2-cix/tools/act/`,
verifies the release checksum, and stores `act` cache data under
`.cache/edk2-cix/act-cache/`.

When invoked from a clean linked Git worktree, the wrapper creates an isolated
local clone for `act`, projects its local and retained remote-tracking source
refs into the canonical branches a published minimised repository exposes, and
binds that disposable snapshot while mounting the shared object store read-only.
This lets `actions/checkout`, nested Docker validation, and
workflow fetches update the disposable clone without moving refs in the
developer's repository. If a normal clone has another local filesystem
repository as `origin`, that exact repository is also mounted read-only;
network remotes need no extra mount. GitHub-only QEMU setup and artifact-upload
steps are skipped under `act`; the local Docker engine must already support the
selected buildbox platform. Documentation runs use the reproducible docs
container under `act`, while GitHub continues to qualify the pinned Nix path.
`.worktrees/` remains untracked host-side scratch state and is never included in
a commit or uploaded as an Actions artifact.

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
make verify-release-branch RELEASE=edk2-202608/cix-1.2/radxa-1.3.1/unofficial
```
