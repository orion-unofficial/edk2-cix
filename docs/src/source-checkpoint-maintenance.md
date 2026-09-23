# Source checkpoint maintenance

Source checkpoints must reconstruct the named vendor release with a compatible
EDK2 uplift and complete custom overlays. Rendering or compilation alone does
not prove release fidelity. Keep source metadata, dependency checks and actual
build qualification together.

## Source changes and orchestration

Keep CI, source rendering, publication, host cache management, report collection
and regression orchestration on `build`. Propagate a helper into retained
Unofficial refs only when it executes inside the rendered firmware tree or is
compiled into firmware. Direct source-tree builds must retain their packaging
and signature guards; an outer output-copy check cannot replace those guards.

Select destination refs explicitly. Before importing a missing topic, compare
patch equivalence and classify it as already represented, release-specific,
obsolete, superseded or genuinely absent. Preserve imported vendor/component
bytes, checkpoint ancestry, required refs and protected compatibility tags.
See root `AGENTS.md` and `MAINTENANCE.md` for the ref and publication rules.

## Overlay completeness

EDK2 can resolve an INF from `src/` but select its module directory from a custom
overlay. Every referenced module sibling must therefore exist in the selected
overlay, including secondary application INFs and C sources. Byte-identical
companions should be symlinks to the matching imported files.

`scripts/check_source_build_inputs.py` checks all retained Unofficial refs,
including normal and experimental overlays. It checks sibling INFs, symlinks,
literal DSC/FDF dependencies, package declarations, ASL headers, toolchain
selection and retained build contracts. It does not fully evaluate EDK2
conditionals or macro-generated paths; real compilation remains necessary.

## Release-specific interfaces

Preserve the selected EDK2 release's interfaces rather than copying a modern
implementation over every historical checkpoint. The structural checks include:

| Interface | Compatibility boundary |
| --- | --- |
| SMBIOS cache fields | Integer representation through 202508; struct representation from 202511 |
| LTO library and CPC namespace | ArmPkg GccLto and Arm CPC through 202405; newer location/namespace from 202408 |
| Vendor PSD type | `AML_PSD_INFO` before 202402; `CIX_AML_PSD_INFO` from 202402 |
| ArmLib, toolchain and capsule dependencies | Follow the selected base's paths, GCC/GCC5 tag and package availability |
| MPAM and console helpers on 202208 | Explicit custom compatibility header and older supported libfdt entry points |

The 202208/1.2.4 and 202208/1.3.1 custom checkpoints start from their matching
Radxa sources. A generic older checkpoint plus a new version string is not a
valid replacement. Input provenance and exact vendor-payload checks reject
such aliases before they can produce a misleading firmware label.

## Correct a checkpoint

Prepare and validate a descendant containing only reviewed source changes, then
use the integration entry point, for example:

```bash
make integrate-source-release \
  TYPE=unofficial RELEASE=1.3.1 EDK2_BASE=edk2-stable202608 \
  REF=<reviewed-descendant> ALLOW_REPLACE=1 WRITE=1
```

Without `WRITE=1`, this is a dry run. Non-descendants and checked-out checkpoint
branches are rejected. The original object remains an ancestor and is recorded
as `maintenance_base_object_id`; this command does not move a development line.
Refresh affected generated caches and manifests, run the qualification gates,
and publish matching source refs with the build metadata using the coordinated
publication workflow in `MAINTENANCE.md`.

Rendered-tree expectations must include release-metadata transformations, even
when a generated cache branch is absent. Hash the final rendered tree rather
than reusing the input checkpoint's tree hash.

## Validation and expansion

Run `make test` and `make lint`, including all-checkpoint input checks and the
minimised-clone reconstruction. Compiler tests cover logging-only RELEASE
headers while keeping assertions and DEBUG-only helpers excluded. Configuration
regressions exercise sequential builds and stale-output guards. The
[CI policy](maintenance-and-ci.md) defines the required full build matrix;
structural checks continue to cover every retained checkpoint.

The primary custom matrix has 16 builds per board: baseline/current EDK2,
Radxa 1.2.4/current, fixes off/on and menus off/on. Stock replay separately
covers the six retained Radxa releases on both boards. These gates do not
qualify every debug mask, distro, core order or host toolchain, and packaging
does not prove boot behavior on hardware.

Use the [unattended release-expansion runner](release-expansion.md) to prepare
additional source pairs and collect actual build results without changing the
maintained policy or publishing candidates automatically.
