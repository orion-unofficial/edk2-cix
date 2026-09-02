# Maintenance Qualification

The root `MAINTENANCE.md` on the default `build` branch is the canonical,
self-contained repository-maintenance guide. This page explains the extended
qualification policy and regression tests that live alongside the build
implementation and are retained by minimised exports.

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
published candidate. The reusable replay and current-source workflows omit
their own concurrency groups: the calling firmware-qualification run owns that
policy, avoiding a caller/callee concurrency deadlock. A direct dispatch with
no concurrency group is also allowed to finish independently.

`Build documentation` runs when its inputs change and deploys Pages only from a
push to `build`. Pull requests, merge-queue candidates, reusable calls, and
local `act` runs build and archive the site without deploying it.

## Build-branch qualification

The build branch contains the tests and every workflow that exercises them.
Its `Firmware qualification` workflow runs `make test` and `make lint` before
the firmware matrices, while `Build documentation` builds this book. The
stable qualification summary is therefore tied to the exact implementation,
metadata, tests, and documentation in the candidate commit.

The extended regression coverage includes:

- atomic publication and immutable-source-ref safeguards;
- event-driven qualification, matrix, concurrency, and documentation-deployment
  policy; and
- upstream-monitor coverage and date-coupled source selection.

The local `act` commands for each build-branch workflow are documented in the
root guide. `make docs-build DOCS_BUILD_MODE=container` is the faster local
check when only documentation changed.

## Validation

Run the same checks locally before publishing a maintenance change:

```bash
make test
make lint
make docs-build DOCS_BUILD_MODE=container
```

Run the source-specific checks listed in root `MAINTENANCE.md` when a candidate
changes source refs, render logic, manifests, or CI.
