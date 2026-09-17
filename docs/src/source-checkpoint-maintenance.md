# Source checkpoint maintenance audit

## September 2026 build failure

The public `edk2-202605/radxa-1.3.1/unofficial` custom build failed because
`FwVersionProtocolTest.inf` was missing from an existing overlay directory.
EDK2 resolved the INF itself from `src/`, but resolved `MODULE_DIR` from the
overlay. GenMake then made the FFS UI section depend on the nonexistent overlay
INF. Checking that the directory contained *some* INF did not catch this.

The failure was reproduced with GenFds multithreading enabled in a Linux
buildbox. Adding the missing INF and C symlinks allowed that configuration to
compile and package with the existing flash partition limits.

## Focused propagation audit

The retained compatibility branches already contained equivalent fixes. The
vendor-line checkpoints and the historical `1.2/current` line had missed them.
The correction preserves their existing ancestry and cherry-picks the relevant
topics, with original commit IDs recorded in each commit message.

| Original topic | Classification and action |
| --- | --- |
| `5f36bab487` | Missing fix: pull an absent buildbox image. Restore everywhere. |
| `65f3abc664` | Missing fix: expose shared Git object stores to the buildbox. Restore everywhere. |
| `24ef31676a` | Missing fix: complete the FwVersion overlay and avoid nested Git mounts. Restore everywhere. |
| `57c8f42fe3` | Missing fix: avoid false PDB-path findings in binary metadata audits. Restore everywhere. |
| `052459dd2b` | Missing fix where `ensure_iasl.sh` exists: keep its stdout machine-readable. Older checkpoints without that resolver do not need it. |
| `0665fdef83`, `8d0b205e14`, `bd8658302f`, `88df23dd94` | Historical vendor replay and ACPI baseline support. Already represented on the compatibility branches used by upstream replay; do not import the entire replay conversion into custom checkpoints. |
| `675827cae0`, `fee1ddbd0a`, `0f29cc276f` | Replay temporary-directory, payload-comparison, and report-ownership fixes. Same replay-specific applicability. |
| `90f6ee3237` | Versioned Microsoft Secure Boot metadata refresh. Preserve checkpoint input versions; this is not the missing custom build-input repair. |

The corrected immutable checkpoints are `1.2.1`, `1.2.2`, `1.2.3`, `1.2.4`,
`1.3.0`, and `1.3.1` on EDK2 `202605`, plus `1.2.4` and `1.3.1` on `202608`.
The historical mutable `1.2/current` line receives the same focused fixes.

The expanded check also found an incomplete experimental `SetupManagerDxe`
overlay across all retained Unofficial refs. Its INF now links to the normal
custom overlay's INF, preserving that module's custom source list while making
the experimental module directory complete. This focused topic is propagated
to both current lines, every checkpoint, and every compatibility branch.

An actual O6N build of `202608 / Radxa 1.2.4` then exposed incomplete EDK2
uplift changes. The affected 1.2 line and retained 202608 compatibility branch
now receive the exception-handler mapping, new library dependencies, ACPI helper
macros, and AML API adaptation already present in the corrected 1.3.1 uplift.
The dependency audit also found the old `ArmPkg` location of `ArmLib` in the
202605 checkpoints for Radxa 1.2.1 through 1.2.4 and the retained 202605/202608
compatibility branches. Those mappings now follow upstream's move to `MdePkg`.
These are focused compatibility corrections, preserving vendor-specific code.

The 202605 Radxa 1.2.4 build also exposed the old `GCC5` toolchain selection
and dependencies on the removed `SignedCapsulePkg`. The four 202605 Radxa 1.2
checkpoints and the retained 202605/202608 compatibility branches now use `GCC`
consistently in their build commands, output paths, and validators. Their CIX
capsule implementation uses the compatibility headers and package declarations
already retained by the 1.2.4 202608 port. Older EDK2 branches that still provide
`GCC5` and `SignedCapsulePkg` keep those dependencies.

These source changes execute inside the rendered firmware tree: EDK2 consumes
the overlay, and that tree invokes the container launcher, iasl resolver, and
firmware metadata audit. They therefore belong on the retained source refs.
Matrix enumeration, source-input verification, caching, and CI policy remain
ordinary changes on `build`.

## Regression gates

`scripts/check_source_build_inputs.py` checks every retained Unofficial source
tree, including checkpoints not selected by the current default. It rejects
missing sibling module INFs, broken overlay symlinks, and loss of the focused
build-fix contracts above. Library INFs are excluded from the FFS UI prerequisite
check because libraries do not produce those sections. Both normal and
experimental overlays are inspected.

The check also follows unconditional literal DSC includes from both supported
boards and verifies their INF dependencies through each overlay configuration.
This catches removed library paths such as the incomplete 202608 uplift.
It also checks CIX/Radxa INF package declarations and verifies that the selected
toolchain has AARCH64 compiler definitions in that checkpoint's BaseTools.
It does not evaluate EDK2 conditional expressions or macro-expanded paths;
the compile jobs remain necessary to validate those and the C/ASL interfaces.

The supported-firmware workflow derives all valid
`edk2-*/radxa-1.2.4/unofficial` and `edk2-*/radxa-1.3.1/unofficial` completions
from the same source model used by the public Makefile. Each target is compiled
and packaged for O6 and O6N with fixes disabled and enabled. It invokes public
`make build`, with `CIX_RELEASE=` and the tester's remaining feature settings.
Adding another EDK2 release expands the matrix automatically. It must be
explicitly sharded if it grows beyond GitHub's 256-job limit; it never silently
drops releases.

Source-model checks cover all allowed release tuples and aliases. These checks
and the all-checkpoint input checks are preflight validation, not evidence that
every possible DEBUG, distro, UART, experimental, core-order, or compiler-host
combination has completed compilation. The full compile matrix above has a
deliberately explicit configuration; qualification reports must preserve that
distinction. Successful packaging also does not establish successful boot on a
device.

## Correcting an existing checkpoint

Prepare and validate a descendant containing only reviewed source changes, then
use the integration entry point:

```bash
make integrate-source-release \
  TYPE=unofficial RELEASE=1.3.1 EDK2_BASE=edk2-stable202605 \
  REF=<reviewed-descendant> ALLOW_REPLACE=1 WRITE=1
```

Without `WRITE=1`, this is a dry run. Non-descendants and checked-out checkpoint
branches are rejected. The original object remains an ancestor and is recorded
as `maintenance_base_object_id`; no development line is moved by this command.
Refresh affected generated caches and manifests, run the qualification gates,
and publish source refs together with the build metadata using the documented
coordinated publication workflow.
