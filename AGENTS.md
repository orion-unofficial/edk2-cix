# Codex Instructions — edk2-cix-orion-unofficial

These repository-specific rules supplement the user's global `AGENTS.md`. Follow both; do not use generic cleanup or optimisation advice to weaken the repository's source-ref, qualification, evidence, or firmware-safety requirements. Prefer the maintained `README.md`, `MAINTENANCE.md`, `docs/src/` and machine-readable manifests for current procedures and pins. Session checkpoints record in-progress state; they do not override repository policy or observable evidence.

## 1. Safety, scope, and authorization

- This repository produces boot-critical firmware for Radxa Orion O6/O6N. Incorrect artifacts can prevent booting or compromise persistent UEFI variables. Never flash firmware, run a firmware updater, reboot hardware, or change Secure Boot keys/variables without explicit user authorization for that operation. Report hardware validation only when recorded evidence establishes what was tested and its result.
- Do not push, force-push, rewrite published refs, create releases, move retained source refs or tags, or dispatch expensive remote jobs unless the user has authorized that operation and its scope. A request to *prepare*, *test*, or *qualify* is not authorization to publish or deploy.
- Ordinary local commits are permitted only when needed and consistent with the requested task; preserve the pre-existing checkout state and clearly identify any commit/ref created. Never commit unrelated or unreviewed work.
- Do not silently broaden an investigation, initiate unrelated release expansion, or loop autonomously through failures. Work in finite, reviewable stages; stop and report a blocker rather than substituting an unapproved operation.
- Before acting, inspect `git status`, the exact checkout/ref and relevant working-tree state. Preserve unrelated changes, other sessions' files, local commits and failed-attempt evidence. Do not presume a nested checkout is disposable or shares the outer repository's history.

## 2. Source-ref model and protected identities

The `build` branch owns the Makefile, source-selection and rendering orchestration, manifests, host-side helpers, documentation, CI workflows and associated tests. The firmware source trees are reconstructed from pinned upstream, vendor, port and Unofficial refs. A passing `build` CI run does **not** establish qualification of unpublished firmware changes.

Treat these as required source data, **not cleanup candidates**:

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
- `refs/tags/source/unofficial/edk2/stable-*`
- `config/refs-*.json`, `config/remotes.json`, `config/policies.json` and other referenced pin/identity manifests.

Never delete, rename, rewrite or garbage-collect away protected refs or required objects to recover space. A source-model migration requires verified replacements that reconstruct the same intended trees and explicit approval before old identities are removed.

`refs/heads/source/cache/release/**` and `refs/heads/source/cache/base/edk2/**` are generated caches, not canonical inputs. Even these may be pruned **only** after `make verify-build-matrix` and representative `make verify-release-branch RELEASE=<source-target>` regeneration establish that targets can be recovered from required refs and manifests without the caches. Do not automatically prune them for disk space.

Check `git rev-parse --show-toplevel --git-dir --git-common-dir` and `git worktree list --porcelain` before changing refs or cleaning worktrees. Worktrees share Git refs and objects with their common repository; pruning shared refs requires explicit user authorization covering that shared repository state. A directory beneath `.worktrees/` may instead be an independent clone or an ordinary build directory; verify which it is.

## 3. Source changes and descendant integration

- Keep build-harness, workflow, documentation and host-side tooling changes on `build` unless the changed code actually must execute *inside* a rendered firmware source tree. Record the reason before propagating harness changes into `source/unofficial/**`.
- Follow [checkpoint maintenance](docs/src/source-checkpoint-maintenance.md) and `MAINTENANCE.md` for reviewed source changes. Use `make integrate-source-release TYPE=unofficial ... ALLOW_REPLACE=1 WRITE=1` for named checkpoint corrections; updates must preserve ancestry and include matching pin/manifest updates and validation. Import development-line changes into the intended `SOURCE_UNOFFICIAL_REF=source/unofficial/<line>/current`. Do not create per-fix `source/unofficial/corrections/**` branches, ad hoc identity aliases, or rewritten checkpoint histories.
- Never equate ancestry with source equivalence. Compare relevant patches/trees (`git cherry`, `git range-diff`, or focused diffs as appropriate) before classifying work as integrated. Equivalently applied changes may have different commit IDs; shared ancestry alone may still be insufficient for functional qualification.
- Never infer that a fix on one release or Radxa line is correct for another. Review boundaries, dependencies, upstream differences and required propagation explicitly. Use `PROPAGATE_RELEASE_BRANCHES=all` only for reviewed cross-release imports, and validate every requested destination candidate before moving refs. Do not replay the entire historical stack as a substitute for focused analysis.
- Respect explicitly frozen inputs, helpers and acceptance criteria for the active qualification attempt. Record their exact identities and the governing plan or manifest. Changes require review and a new qualification attempt; preserve the original evidence. Do not quietly modify frozen helpers or weaken criteria to obtain a passing test.
- Never refresh pin metadata simply to match whatever refs happen to be present locally. In particular, do not use `make refresh-source-metadata` to mask a CI source-coherence failure. Investigate the intended exact identities.

For an authorized source-model publication, follow `MAINTENANCE.md`: run the `make publish-source-update` dry run, preserve source/object/tree and author/committer identity checks, and publish the build metadata and affected source refs together with exact leases in one atomic transaction. Run `make check-remote-source-coherence` afterward. `FORCE=1` bypasses only the publisher's commit-identity guard, not its other safety checks. These procedures do not authorize publication.

## 4. Firmware provenance and qualification

Record and verify exact source **commits and tree IDs**, component and payload inputs, release and board selections, compiler/container identities, firmware hashes, test criteria, run IDs and evidence paths where relevant. Branch names and mutable tags are not sufficient evidence of the input that was actually tested.

Distinguish these states; never collapse them into a generic "passed":

1. **Integration unverified** — a target exists, but inclusion/equivalence of required candidate changes has not been established.
2. **Qualified isolated candidate** — specific isolated inputs and outputs passed a defined set of tests; this does not qualify maintained refs.
3. **Integrated, unqualified descendant** — required changes were verified in the maintained descendant, but required qualification has not completed.
4. **Qualified descendant** — that exact maintained source/configuration and its pinned dependencies independently passed every applicable acceptance criterion.

Keep **failed**, **blocked**, **skipped**, **incomplete**, **unverified**, and **known inherited/expected failure** separate from **passed**. A successful process exit, partial matrix, stale run, historical artifact, or different source revision cannot establish final acceptance. Preserve failed-attempt evidence and expected-failure controls; never weaken checks or reclassify failures solely to obtain green CI.

Follow the maintained acceptance criteria for each affected component and its producer/consumer interfaces; see [StandaloneMM qualification](docs/src/standalone-mm.md) for that component's compatibility, identity, storage and behavioral requirements. Do not substitute size/hash/count checks for required byte-level or behavioral validation. Full-image, emulated and physical-hardware results are different evidence classes. No emulator-only result proves O6/O6N boot, UART, SPI-NOR persistence or Secure Boot behavior on actual hardware.

Do not hard-code fast-changing release pins, candidate commits, historical run states or one-off acceptance counts in this file. Read `MAINTENANCE.md`, the relevant maintained component and CI documentation in `docs/src/`, and current `config/` and session manifests instead.

## 5. Execution, CI, and token efficiency

- Prefer GitHub Actions for reproducible, expensive or parallel firmware builds/tests when the required source objects, pinned dependencies, runner resources and evidence capture are actually available. Use local work only for focused changes, lightweight checks, unsupported hardware/emulator requirements, or an explicitly approved build.
- Use supported Makefile and documented scripts; consult `make help`, `make help-vars`, and the relevant maintenance/CI documentation before constructing ad hoc workflows. Do not invent weaker replacement qualification gates.
- Follow the affected component's documented qualification stages and prerequisites: input/identity preflight precedes applicable build and behavioral checks, followed by qualification of the exact integrated descendant. A workflow's existence does not establish that every stage is supported; consult its maintained component and CI documentation, such as [StandaloneMM qualification stages](docs/src/standalone-mm.md#qualification-stage-progression).
- Before a workflow dispatch, confirm that the workflow is published on the required ref and that the checkout SHA and workflow revision satisfy its identity policy, including `harness_sha` where required by that workflow. Do not silently advance to a mutable branch tip or substitute available inputs for missing candidate objects.
- Use consistent published floating major-version tags for GitHub Actions dependencies; compatible upstream updates are adopted automatically. Pin significant OCI/compiler inputs to reviewed immutable identities when required. Verify registry accessibility, runner architecture, memory and disk capacity. Preserve the distinctions between Linux CI runners, the macOS host, locally built images and physically tested firmware.
- Prefer `fail-fast: false` only for independent qualifications where collecting all results is valuable. Summary jobs must fail closed on missing, stale, duplicate or contradictory evidence; upload failures cannot be treated as success. Archive necessary artifacts before CI retention expiry.
- Run focused regression checks after a small fix; reserve expensive complete matrices for meaningful qualification checkpoints. A rerun of an old workflow tests its original commit, not an unpublished fix.
- Do not spend model invocations polling unchanged jobs. Prefer job state files, GitHub Actions completion, event-driven notification, or a later manually requested analysis. Do not duplicate a running job. Read concise manifests and failure summaries before large logs. Avoid recursive speculative investigations and autonomous retry loops.

## 6. Session workspaces, scratch and resource budgets

Apply the global workspace and resource-management rules. In this repository use:

- `.agent-work/session-<id>/` for small persistent checkpoints, provenance, receipts, failure evidence and job state;
- `.worktrees/session-<id>-<purpose>/` for substantial isolated source trees and resumable builds;
- `.cache/` or an established configured cache location for reproducible, bounded caches;
- session-labelled OS temporary directories only for disposable scratch whose complete loss is acceptable.

Use the full session ID when available, otherwise a descriptive collision-resistant identifier. Keep unique changes, required evidence and firmware artifacts out of ephemeral storage. Reuse suitable existing checkouts and outputs. Point `TMPDIR`, `TMP`, `TEMP` or `EDK2_CIX_TMP_ROOT` to persistent storage only when the tools require resumable scratch.

Before multi-gigabyte operations, measure free space and relevant build/cache/container usage, estimate peak allocation, check for equivalent active jobs and reusable inputs, and consider suitable CI. Retain the global default of at least 10 GiB free at peak unless an alternative budget is explicitly agreed. Track significant owned resources, jobs, approximate sizes, evidence and cleanup conditions in the session checkpoint. Stop creating large artifacts and investigate if space falls unexpectedly; do not delete protected source data to make room.

## 7. Cleanup and retention

Apply the global ownership, active-use and recoverability checks before cleanup. Preserve unpublished Git objects, pinned or non-reconstructible inputs, firmware candidates, required qualification artifacts, failed-attempt evidence and current checkpoints. Record exact inputs, output hashes and necessary evidence before removing owned reproducible build intermediates. Qualification archives and storage fixtures are not disposable merely because they are old or large.

Verify whether a directory is a linked worktree or an independent clone before removal. Use `git worktree remove` for registered worktrees; `git worktree prune` only removes stale administration records. Do not use blanket `git clean -fdx`, `rm -rf .worktrees`, global container/volume pruning, aggressive Git GC or forced removal as routine cleanup. Protect the refs and regeneration gates in section 2. If ownership or recoverability is uncertain, retain the resource and report why.

## 8. Absolute patch-file rule

Treat source patch files (`*.patch`, `*.diff`) as **generated artifacts, not source files**. Never hand-edit patch hunks, hunk headers, context, added/removed lines or line counts; never assemble or repair hunks by eye or apply `apply_patch` to a patch-file hunk body. Editing **actual source files** with `apply_patch` is allowed.

When changing a patch:

1. Reconstruct the **exact source-tree preimage** that the patch targets.
2. Apply preceding patches in stack order.
3. Edit only the real source files.
4. Regenerate the patch mechanically using `git diff -U3`, `diff -U3`, or an appropriate equivalent, retaining at least three context lines where file boundaries permit.
5. Preserve valid diff metadata by default. If the consumer requires omitting `index <old>..<new> <mode>` lines, remove **only those complete top-level metadata lines immediately following `diff --git`**, never hunk contents.
6. Dry-run/check the regenerated patch against a clean preimage using `git apply --check`, `patch --dry-run`, or an appropriate equivalent.
7. Only then update patch-stack lists and associated manifests/checksums or remove an obsolete patch from its stack.

If the exact preimage cannot be reconstructed, **stop and report the blocker**. Do not approximate. If an earlier regenerated patch makes a later one redundant, remove the redundant later patch and its metadata instead of hand-rebasing it. Only whole-file mechanically generated replacements, deletions of obsolete patch files, or edits to non-diff preamble text are permitted for patch-file contents.

## 9. Source integrity and documentation

Preserve exact imported/vendor files, byte-sensitive fixtures, binaries, cryptographic data and symlink structure. Use LF for project-owned new text/scripts, but avoid incidental line-ending normalization or whole-file churn in upstream-derived material.

Maintain supported, current procedures in `README.md`, `MAINTENANCE.md` and `docs/src/`. Place supported code and tests in `scripts/` and active pin/identity records in `config/`. Keep one-off audit trails and session-specific evidence under ignored `.agent-work/session-<id>/`; do not embed ephemeral session IDs, local absolute paths, stale run results or private evidence links in published documentation.

Keep maintained manifests minimal and functional. Do not add permanent historical identity maps, abandoned correction namespaces or lengthy duplicated ledgers when Git history and session evidence suffice. Before retiring a session note, migrate any **still-current** engineering constraint into maintained documentation.

## 10. Completion and fresh-session handover

For substantive work, persist a concise checkpoint with the objective, exact source/configuration and evidence identities, completed checks, remaining qualification, active jobs, authorization limits and next bounded action. Report changes, validation, qualification state, blockers, and the cleanup/resource summary required by the global instructions, including significant retained paths, sizes, retention reasons and material disk-space changes. A fresh session must be able to resume without relying on chat history. Small tasks need no elaborate workspace or handover.

Stop at the requested milestone. Do not resume a paused automation, initiate a new workflow, publish a ref, or flash/reboot a device without authorization.
