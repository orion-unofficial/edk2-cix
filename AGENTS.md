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
- `refs/heads/source/component/cix/**`
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

Changes to `source/unofficial/current` are not automatically known-good for
every EDK2 release branch. When importing a source change that should
apply across supported releases, use the explicit import workflow with
`PROPAGATE_RELEASE_BRANCHES=all` and validate every release branch before
moving refs.

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

## Ephemeral Worktrees And Temporary Data

Treat Git worktrees as ephemeral execution state, not as durable storage. A
worktree under an operating-system temporary location may disappear on reboot,
under storage pressure, or during normal temporary-directory cleanup. If a task
needs a branch checked out as a temporary worktree, remove that worktree once
the task is complete instead of leaving it behind for manual and uncertain
cleanup.

Do not store unique work only in a temporary worktree. Commit, stash, bundle,
or otherwise preserve any useful data in an associated Git repository before
assuming it is safe to remove the worktree.

All directly agent-created ephemeral data must live under a directory named
after the active session id provided by the execution harness. For example,
after assigning that value to `AGENT_SESSION_ID`, a temporary root may be:

```bash
: "${TMPDIR:?set TMPDIR to a writable temporary root first}"
: "${AGENT_SESSION_ID:?set AGENT_SESSION_ID to the active session id first}"
${TMPDIR%/}/edk2-cix-agent/${AGENT_SESSION_ID}
```

When invoking scripts or tools that may create temporary files, set the
temporary environment variables to that session-specific root, for example:

```bash
: "${TMPDIR:?set TMPDIR to a writable temporary root first}"
: "${AGENT_SESSION_ID:?set AGENT_SESSION_ID to the active session id first}"
export TMPDIR="${TMPDIR%/}/edk2-cix-agent/${AGENT_SESSION_ID}"
export TMP="$TMPDIR"
export TEMP="$TMPDIR"
mkdir -p "$TMPDIR"
```

The intent is that every worktree, temporary file, build scratch directory,
download cache, log, archive, and other non-repository artefact written by an
agent can be traced back to the agent session that created it. If
the execution harness does not expose a session id, create a clearly labelled
substitute id and record it in user-facing progress or final output before
creating temporary state.

## Before Pruning

Before deleting any branch or tag, check whether the checkout is a linked
worktree:

```bash
git rev-parse --show-toplevel --git-dir --git-common-dir
```

If the common Git directory belongs to another checkout, deleting a branch or
tag will affect that checkout too. In that case, do not prune refs unless the
user has explicitly authorised pruning the shared repository state.
