# Maintenance and CI

The default `build` branch is the end-user interface and the GitHub Actions
control plane. This `test` branch adds maintainer documentation and regression
tests. Keep `test` rebased or fast-forwarded to the exact `build` candidate
being reviewed; a green `test` run otherwise says nothing about a newer build
tip.

GitHub requires scheduled and manually dispatched workflows to exist on the
default branch, so the small workflow entry points remain on `build`. The
additional `test-branch-ci.yaml` workflow exists only here and runs when `test`
itself is updated or a pull request targets `test`.

## Qualification policy

`Firmware qualification` runs for every push to `build`, pull request targeting
`build`, merge queue candidate, and manual dispatch. It classifies the complete
candidate range. Documentation- and licensing-only changes stop after the
classifier; unknown or firmware-affecting paths run all of these gates:

- source-model tests, lint, and minimised-clone reconstruction;
- current-source Trixie builds for O6 and O6N, both with firmware fixes disabled
  and enabled; and
- exact Radxa 1.3.1 replay on its historical EDK2 `202208` base for O6 and O6N.

The stable `Qualification summary` job reports the result of every gate and
fails if any required gate failed. Branch protection should require that job,
not a matrix child whose displayed name may change. A run qualifies the
candidate tip represented by that run; it is not a claim that every historical
commit was independently built.

Pull-request runs are cancelled when superseded. Published build pushes,
manual builds, exact replays, and current-source matrices are not cancelled by
newer runs, so a later update cannot erase qualification of an already
published candidate.

`Build documentation` runs when its inputs change and deploys Pages only from a
push to `build`. Pull requests, merge-queue candidates, reusable calls from
`test`, and local `act` runs build and archive the site without deploying it.

## Scheduled monitoring

Only `Upstream versions` and Dependabot are time based. Firmware qualification,
exact replay, and current-source builds run when repository inputs change,
rather than repeating weekly against identical inputs.

The upstream checker covers every configured source remote plus build tooling:
EDK2 and its Platforms/Non-OSI companions, Radxa firmware, CIX BIOS,
bootloader1, TF-A and OP-TEE forks, Arm TF-A and OP-TEE, Microsoft Secure Boot
payloads, GitHub Actions, `act`, its runner image, Nix, and mdBook tooling. A
source remote must have at least one check or an explicit `monitor: false`
record with a reason.

Published releases and immutable/date-coupled selections are strict. Floating
development heads and independently moving Arm upstreams are advisory because
they signal review work rather than a mechanically safe upgrade. In particular,
the selected EDK2 Platforms and Non-OSI commits must be the latest commits at
or before the corresponding EDK2 stable-tag timestamp.

Run the same checker locally with:

```bash
make check-upstream-versions
make check-upstream-versions UPSTREAM_VERSION_MODE=advisory
make check-upstream-versions UPSTREAM_VERSION_ONLY=edk2,radxa
```

## Publishing source updates

A source integration changes both a `source/**` ref and its hash-bearing build
metadata. Publish them atomically from a clean `build` checkout:

```bash
make publish-source-update \
  SOURCE_REFS=source/base/edk2/edk2-stable202608
make publish-source-update \
  SOURCE_REFS=source/base/edk2/edk2-stable202608 \
  WRITE=1
```

Comma-separate all refs belonging to one metadata commit. The command verifies
that local object and tree IDs match the recorded manifest, that the metadata
file changed, that `build` is a fast-forward of the remote branch, and that an
immutable remote source ref is not being replaced. It then uses one atomic push
for `build` and the selected refs. Mutable Unofficial tips use exact
force-with-lease expectations. A dry run is the default.

The command does not publish `test`. After the source update is on `build`,
fast-forward or rebase `test` onto that exact commit, update any test-only
coverage, and publish `test` separately after its validation succeeds.

## Local workflow execution

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

Executable jobs run from a clean isolated snapshot beneath
`.cache/edk2-cix/act-workspaces/`. The invoking checkout is never exposed to
workflow mutations. `make gha-act-run` therefore refuses a dirty checkout.
The shared Git object store is mounted read-only; `.worktrees/` is neither
mounted into the job nor committed or uploaded. GitHub-only QEMU setup and
artifact uploads are skipped under `act`, so the local Docker engine must
already support the selected buildbox platform.

The local documentation container reuses `.cache/edk2-cix/docs/` and copies the
generated site into the isolated workflow snapshot. This is deliberate cache
state, not source or a `.worktrees` mount.

`manual-firmware-build.yaml` uses workflow inputs rather than a matrix:

```bash
make gha-act-dry-run \
  ACT_WORKFLOW=.github/workflows/manual-firmware-build.yaml \
  ACT_JOB=firmware \
  ACT_EXTRA_ARGS='--input make_target=buildbox-firmware-stage --input board=O6 --input firmware_target=RELEASE --input firmware_distro=trixie --input enable_firmware_fixes=false'
```

For matrix workflows, filter one entry with `ACT_MATRIX`:

```bash
make gha-act-dry-run \
  ACT_WORKFLOW=.github/workflows/secure-boot-audit.yaml \
  ACT_JOB=secure-boot \
  ACT_MATRIX=board:O6 \
  ACT_EXTRA_ARGS='--input firmware_distro=trixie'
```

Replace `gha-act-dry-run` with `gha-act-run` for a real local execution.

## Validation

For normal firmware building, the selected target performs its own preflight
checks. When changing source refs, render logic, manifests, or CI, run:

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

Run `make refresh-help-cache` before `make check-help-cache` when help text or
the derived source-target list changes. Build the documentation with
`make docs-build`; use `DOCS_BUILD_MODE=container` to require the same
containerised local path used by `act`.
