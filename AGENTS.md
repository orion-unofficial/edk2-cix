# Agent Guardrails

This branch contains the build orchestration and source-ref model for producing
EDK2-CIX firmware source targets. Treat the source refs below as required source data,
not as scratch branches.

## Required Refs

Future agents must not under any circumstance delete, rewrite, rename,
garbage-collect away, or otherwise invalidate any of these refs as a clean-up,
pruning, or space-saving operation. If the user explicitly requests a
source-model migration, first create and prove replacement refs that reconstruct
the same firmware source trees, then ask for explicit approval before removing
the old refs:

- `refs/heads/build`
- `refs/heads/source/base/edk2/**`
- `refs/heads/source/base/edk2-platforms/**`
- `refs/heads/source/base/edk2-non-osi/**`
- `refs/heads/source/base/arm/**`
- `refs/heads/source/component/cix/**`
- `refs/heads/source/vendor/radxa/**`
- `refs/heads/source/port/radxa/**`
- `refs/heads/source/unofficial/**`
- unofficial compatibility tags under `refs/tags/source/unofficial/edk2/stable-*`

The manifests and policies on this branch are also required source metadata:

- `config/refs-*.json`
- `config/remotes.json`
- `config/policies.json`

These refs and files are the minimum source material needed to deterministically
reconstruct supported firmware source targets if generated release branches are absent.
Do not treat them as temporary branches merely because they live under a `source/`
namespace.

## Generated Refs

`refs/heads/source/cache/release/**` refs are generated firmware artefacts. They are
useful cached, inspectable materialisations, but they are not irreducible source
inputs. The canonical repository may omit them. If they are present locally, they
may be deleted as caches only after `make verify-build-matrix` confirms the
source targets are derivable and a representative `make verify-release-branch
RELEASE=<source-target>` can regenerate a missing source target from the required refs above.

`refs/heads/source/cache/base/edk2/**` refs are generated EDK2 component skeleton
caches. They may likewise be omitted when `config/refs-edk2.json` and the
referenced EDK2 component refs are present.

## Before Pruning

Before deleting any branch or tag, check whether the checkout is a linked worktree:

```bash
git rev-parse --show-toplevel --git-dir --git-common-dir
```

If the common Git directory belongs to another checkout, deleting a branch or tag
will affect that checkout too. In that case, do not prune refs unless the user has
explicitly authorised pruning the shared repository state.
