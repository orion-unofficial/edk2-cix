# Extended Maintenance Qualification

The root `MAINTENANCE.md` on the default `build` branch is the canonical,
self-contained repository-maintenance guide. This page documents only the
extended qualification material that exists on `test`: additional regression
tests, their workflow, and the branch-protection result they produce.

Keep `test` merged with the exact `build` candidate being reviewed. A green
`test` run otherwise says nothing about a newer build tip.

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
push to `build`. Pull requests, merge-queue candidates, reusable calls from
`test`, and local `act` runs build and archive the site without deploying it.

## Test-branch workflow

GitHub requires scheduled and manually dispatched workflows to exist on the
default branch, so firmware qualification, release replay, manual builds,
documentation, and upstream monitoring remain defined on `build`. Their
maintenance and local `act` commands are documented in the root guide.

The additional `test-branch-ci.yaml` workflow runs the test branch's extended
suite and the shared documentation build, then reports one stable
`Test qualification summary` result. Run its maintenance job locally with:

```bash
make gha-act-dry-run \
  ACT_WORKFLOW=.github/workflows/test-branch-ci.yaml \
  ACT_JOB=maintenance-tests
make gha-act-run \
  ACT_WORKFLOW=.github/workflows/test-branch-ci.yaml \
  ACT_JOB=maintenance-tests
```

The full workflow also calls the reusable documentation job. The normal
`make docs-build DOCS_BUILD_MODE=container` command is the faster local check
when only the test-branch documentation changed.

## Validation

The additional test files on this branch are discovered by the same commands as
the build-branch suite:

```bash
make test
make lint
make docs-build DOCS_BUILD_MODE=container
```

Run the source-specific checks listed in root `MAINTENANCE.md` when the merged
build candidate changes source refs, render logic, manifests, or CI. The test
branch supplements those checks; it does not replace them.
