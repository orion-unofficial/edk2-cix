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
  and enabled; the O6/fixes-on lane then builds a CIX V1.2 source-signed image
  after its stock-payload image and verifies both certificates and BL1 identity;
  and
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
compiler options or packaging. All twelve stock replay builds must pass CI before
claiming that all stock replays have been requalified at a candidate commit.

The stable `Qualification summary` job reports the result of every gate and
fails if any required gate failed. Branch protection should require that job,
not a matrix child whose displayed name may change. A run qualifies the
candidate tip represented by that run; it is not a claim that every historical
commit was independently built.

See the [checkpoint maintenance guide](source-checkpoint-maintenance.md) for
overlay completeness, release-specific interfaces and validation limits.

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
  policy;
- upstream-monitor coverage and date-coupled source selection;
- native execution of the selected custom updater and setup callbacks with
  allocation, I/O, malformed-variable and readback failures injected;
- native PPTT cache-attribute checks against the selected ACPI structures; and
- configuration-protocol entry bounds, typed writes, allocation and installation
  failures, including requests through copied entry descriptors, invalid parser
  arguments and parser-buffer cleanup.

The native C tests use compiler warnings as errors and address/undefined-behavior
sanitizers. A source/header fingerprint groups identical retained variants and
runs the checks once for each distinct interface, including the legacy FMP and
ACPI 6.3 paths. They complement real firmware builds; they cannot prove the behavior
of binary flash utilities or establish successful booting.

### Warnings and audit differences

Custom builds retain compiler warnings as errors and fatal AutoGen warnings,
with the existing toolchain exceptions. The AArch64 `-Wno-lto-type-mismatch`
exception is also present in [EDK2's tool definitions](https://github.com/tianocore/edk2/blob/edk2-stable202608/BaseTools/Conf/tools_def.template):
mixed BASE/XIP and DXE alignment options can trigger this diagnostic for identical
GUID and packed-data declarations. It also disables that diagnostic for genuine
cross-object type mistakes; a passing link is not proof of LTO type correctness.
Keep the existing module contexts and alignment rules when investigating it.

Exceptions for variables used only by disabled debug macros and the intermediate
ELF's RWX segment remain explicit in the source build recipe. Upstream quiet mode
may suppress identified vendor diagnostics to preserve reproducible source and
artifacts; unknown warnings and errors remain visible. `V=1` exposes command
output, while `DEBUG=1` also exposes orchestration diagnostics.

ACPI and final-FV audit failures print a bounded list of changed fields and
retain the full differences in their JSON report. FFS entries are matched by
GUID so an inserted module does not make every following module appear changed.
These diagnostics do not relax the existing baseline comparison.

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

## Documentation and helper scripts

`docs/src/` contains maintained user and maintainer guidance. Historical agent
ledgers, one-off audit programs and local qualification evidence belong under
ignored `.agent-work/session-<session-id>/`; execution checkouts and build state
belong under `.worktrees/`. Preserve useful constraints in the maintained guides
before archiving a ledger. Published docs must not depend on those local paths.

`scripts/` contains supported Make/CI entry points, standalone maintenance
commands, imported Python modules, source-propagated build helpers and regression
tests. Not every file needs its own Make target:

- `make test` discovers `test_*.py` through `quality_checks.py`;
  `test_support.py` provides shared test adapters.
- `build_bl33.py`, `warn_debug_categories.py`, `prepare_release_logging.py` and
  `check_release_debug.py` execute inside rendered firmware builds. Their
  build-branch copies support tests and source-contract checks.
- `qualify_bootloader1_signatures.py` and `qualify_source_trusted_firmware.py`
  provide repeatable CI/maintainer qualification commands.
- `release_expansion.py` is the supported standalone
  [unattended batch command](release-expansion.md); its source-construction
  module is not a separate end-user interface.

Repository-owned scripts, docs and new text use LF. Preserve exact imported
vendor files, unchanged mirror symlinks and byte-sensitive replay fixtures.
Do not normalize entire upstream trees or OpenSSL fixtures as cosmetic cleanup;
a deliberate normalization migration needs separate replay/build checks.
