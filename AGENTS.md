# Agent Guardrails

This branch contains the build orchestration and source-ref model for producing
EDK2-CIX firmware source targets. Treat the source refs below as required
source data, not as scratch branches.

## Required Refs

Future agents must not under any circumstance delete, rewrite, rename,
garbage-collect away, or otherwise invalidate any of these refs as a clean-up,
pruning, or space-saving operation. If the user explicitly requests a
source-model migration, first create and prove replacement refs that
reconstruct the same firmware source trees, then ask for explicit approval
before removing the old refs:

- `refs/heads/build`
- `refs/heads/source/base/edk2/**`
- `refs/heads/source/base/edk2-platforms/**`
- `refs/heads/source/base/edk2-non-osi/**`
- `refs/heads/source/base/arm/**`
- `refs/heads/source/vendor/cix/**`
- `refs/heads/source/port/cix/**`
- `refs/heads/source/vendor/radxa/**`
- `refs/heads/source/port/radxa/**`
- `refs/heads/source/unofficial/**`
- unofficial release tags under
  `refs/tags/source/unofficial/edk2/stable-*`

The manifests and policies on this branch are also required source metadata:

- `config/refs-*.json`
- `config/remotes.json`
- `config/policies.json`

These refs and files are the minimum source material needed to
deterministically reconstruct supported firmware source targets if generated
release branches are absent. Do not treat them as temporary branches merely
because they live under a `source/` namespace.

## Generated Refs

`refs/heads/source/cache/release/**` refs are generated firmware artefacts.
They are useful cached, inspectable materialisations, but they are not
irreducible source inputs. The canonical repository may omit them. If they are
present locally, they may be deleted as caches only after the
`make verify-build-matrix` target confirms the source targets are derivable and
a representative `make verify-release-branch RELEASE=<source-target>` can
regenerate a missing source target from the required refs above.

`refs/heads/source/cache/base/edk2/**` refs are generated EDK2 component
skeleton caches. They may likewise be omitted when `config/refs-edk2.json` and
the referenced EDK2 component refs are present.

## Source Change Propagation

Changes to a mutable `source/unofficial/<line>/current` ref are not
automatically known-good for another firmware line or for every retained EDK2
release branch. Select the destination explicitly with
`SOURCE_UNOFFICIAL_REF=source/unofficial/<line>/current`. When a focused source
change should also apply across the retained historical EDK2 release branches,
use the explicit import workflow with `PROPAGATE_RELEASE_BRANCHES=all` and
validate every release branch before moving refs.

Do not use a broad historical replay as a substitute for review. If a change
appears to exist in one release branch but not another, first audit with Git's
patch-equivalence tools, for example `git cherry`, then classify each
non-equivalent commit as one of:

- already represented by a release-specific replay;
- intentionally release-specific or obsolete;
- superseded by a newer implementation;
- a genuine missed fix that should be re-imported as a focused topic.

The fiptool quiet-output repair was a missed focused fix: the 202208 release
branch contained earlier quiet-output work, while later release branches had
independently
created fiptool source-build commits that never received that helper. Future
agents should treat this as a propagation-audit lesson, not as permission to
rewrite release-branch history wholesale.

## Patch Formatting

When creating or revising context diffs, every hunk must include at least three
unchanged context lines before and after the changed region. The only
exceptions are hunks whose changed region is within three lines of the start or
end of the file, where the missing context physically cannot exist. Free-form
preamble before the first `diff --git` line and trailing commentary after the
last diff are fine, but they do not count as context for any hunk.

Avoid hand-assembling diff hunks in memory when a safer path is available:
this has repeatedly produced plausible-looking patches with invalid hunk
headers or unsafe context. Wherever possible, construct patches from real file
states instead. Preferred workflows are making the change in a scratch checkout
and capturing it with `git diff`, or creating original and modified file copies
and running `diff -u` between them. After capturing the patch, undo any scratch
edit that was made only to generate the diff. If an existing hunk lacks the
required context, reformulate it before delivering or committing it.

## Ephemeral Worktrees And Temporary Data

Treat operating-system temporary locations as strictly ephemeral. Do not put
resumable work, conflict resolutions, build logs needed for audit, or other
unique task state there. Prefer a clearly named directory beneath `.worktrees/`
at the shared repository root for worktrees and scratch that may need to survive
a crash, reboot, cleanup job, or session reset.

Git worktrees remain execution state rather than the sole durable record of
completed work. Commit, stash, bundle, or otherwise preserve useful changes in
the associated Git repository before removing a worktree.

All directly agent-created scratch must live under a directory named after the
active session id where the execution harness provides one. For example, from
the shared repository root:

```bash
: "${AGENT_SESSION_ID:?set AGENT_SESSION_ID to the active session id first}"
scratch_root="$(git rev-parse --path-format=absolute --git-common-dir)/../.worktrees/session-${AGENT_SESSION_ID}-scratch"
mkdir -p "$scratch_root/tmp"
```

When invoking scripts or tools that create temporary files, point both the
generic temporary variables and the repository helper at that root:

```bash
: "${AGENT_SESSION_ID:?set AGENT_SESSION_ID to the active session id first}"
scratch_root="$(git rev-parse --path-format=absolute --git-common-dir)/../.worktrees/session-${AGENT_SESSION_ID}-scratch"
export TMPDIR="$scratch_root/tmp"
export TMP="$TMPDIR"
export TEMP="$TMPDIR"
export EDK2_CIX_TMP_ROOT="$TMPDIR"
mkdir -p "$TMPDIR"
```

Use an operating-system temporary location only when complete loss is
acceptable. Even then, group directly created files beneath one session-labelled
parent and remove them promptly. Keep a running list of scratch paths, remove
only paths created for the current task, and report anything intentionally
retained at handoff. If the harness exposes no session id, create a clearly
labelled substitute and record it before creating scratch state.

## Before Pruning

Before deleting any branch or tag, check whether the checkout is a linked
worktree:

```bash
git rev-parse --show-toplevel --git-dir --git-common-dir
```

If the common Git directory belongs to another checkout, deleting a branch or
tag will affect that checkout too. In that case, do not prune refs unless the
user has explicitly authorised pruning the shared repository state.
