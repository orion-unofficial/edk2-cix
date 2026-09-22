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
- overlay, symlink, and required build-fix checks across every retained
  Unofficial checkpoint;
- the primary `edk2-{202208,202608}/radxa-{1.2.4,1.3.1}/unofficial`
  targets through public `make build`, with fixes off/on and experimental
  menus off/on (RELEASE, Trixie, vendor early-boot payloads retained): 16 O6
  builds and 16 O6N builds;
- current-source Trixie builds for O6 and O6N, both with firmware fixes disabled
  and enabled; and
- exact replay of stock Radxa 1.2.1, 1.2.2, 1.2.3, 1.2.4, 1.3.0 and 1.3.1 on
  EDK2 `202208` for O6 and O6N: **12 stock replay builds**.

`firmware_qualification_policy` keeps EDK2 202208 and Radxa 1.2.4 as fixed custom
baselines. The other custom axis values come from the deliberately selected
`unofficial_source_policy` line, currently EDK2 202608 and Radxa 1.3.1. Promotion
therefore changes the required four pairs together; missing source integrations
fail matrix enumeration. It does not automatically promote discovered tags.
The stock replay list is cumulative: add the promoted Radxa release and its
replay inputs while retaining earlier supported releases. Enumeration rejects a
current custom Radxa version missing from stock coverage, or a stock target
whose base is not its exact vendor ref.

The stock workflow keeps the existing upstream build and byte-comparison path.
Expanding its callers does not change vendor source bytes, warning policy,
compiler options or packaging. The new twelve-build coverage must pass CI before
claiming that all stock replays have been requalified at a candidate commit.

The stable `Qualification summary` job reports the result of every gate and
fails if any required gate failed. Branch protection should require that job,
not a matrix child whose displayed name may change. A run qualifies the
candidate tip represented by that run; it is not a claim that every historical
commit was independently built.

See the [checkpoint maintenance audit](source-checkpoint-maintenance.md) for
the regression that motivated this coverage and its precise validation limits.

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

The primary matrix keeps the stock firmware's 202208 base and current 202608
base. The 202605 base is superseded for routine qualification. Historical source
refs remain available, but mismatched custom aliases are excluded from public
help and fail the provenance guard. Passing compilation does not establish
boot behavior: only O6 is currently available for qualification on hardware;
O6N remains covered by CI builds.

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
