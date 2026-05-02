# EDK2-CIX Firmware Build

This branch contains the Makefile, manifests, and scripts used to select source versions and build firmware for the Radxa Orion O6 and O6N boards. The source model combines:

- upstream bases such as EDK2, TF-A, and OP-TEE
- Radxa and CIX vendor source layers
- local project changes
- generated materialised firmware worktrees that are ready to build

Users who want to build firmware, rather than change the source model, should find that the `make` targets provide the flexibility they need. In this document, a Git "ref" means a name that identifies a commit, such as a branch, tag, or commit ID. Start with:

```bash
make help
make help-vars
make help-variants
```

## How do I build the latest firmware?

The default firmware variant is derived from the latest available supported EDK2 release, CIX release, Radxa release, and local source checkpoint. The supported variant set is generated from local source refs such as `source/base/edk2/edk2/**`, `source/delta/radxa/**`, `source/component/cix/**`, and the source/ref manifests under `config/refs/`. If you do not set `RELEASE=...`, the build targets use that derived default variant. User-facing build and packaging targets use the buildbox by default, so the host does not need an AArch64 firmware toolchain installed.

For a normal single firmware build, choose the board and build target:

```bash
make buildbox-firmware-build FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE
make buildbox-firmware-build FIRMWARE_BOARD=O6N FIRMWARE_TARGET=RELEASE
```

Common variables are:

- `FIRMWARE_BOARD=O6|O6N` selects the board. The default is `O6`.
- `FIRMWARE_TARGET=RELEASE|DEBUG` selects a release or debug firmware image. The default is `RELEASE`.
- `RELEASE=<variant>` selects a configured firmware variant. Leave this unset to use the latest derived variant.
- `ARTEFACT_MODE=custom|upstream` selects the build and artefact mode inside the chosen source tree. The default is `custom`.
- `V=1` enables verbose script and delegated build output. The default, `V=0`, keeps output concise.

Most users should leave `ARTEFACT_MODE=custom`. It enables the local firmware build switches exposed by this project. `ARTEFACT_MODE=upstream` is for vendor-style comparison or qualification builds; it does not change the selected source variant, and it rejects local custom-only feature variables. Use `RELEASE=...` to choose source versions.

To stage the deployable payload under `dist/firmware/`, use:

```bash
make buildbox-firmware-stage FIRMWARE_BOARD=O6N FIRMWARE_TARGET=RELEASE
```

To create a single-board archive for the selected firmware variant, use:

```bash
make zip FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE
make targz FIRMWARE_BOARD=O6 FIRMWARE_TARGET=RELEASE
```

`make install` builds and stages one selected payload, then checks the selected install root before copying anything. By default it installs under `/boot/efi`; set `INSTALL_ROOT=/boot` or another path if your system uses a different mount point. Existing firmware payload files under `INSTALL_ROOT` are never replaced unless you rerun with `FORCE=1`.

```bash
make install FIRMWARE_BOARD=O6 INSTALL_ROOT=/boot/efi
make install FIRMWARE_BOARD=O6 INSTALL_ROOT=/boot/efi FORCE=1
```

`make build-all` is for producing a distributable bundle containing all supported firmware variants for the selected board. It deliberately ignores most single-variant selection variables because it chooses the supported variant set itself.

```bash
make build-all FIRMWARE_BOARD=O6
```

Exact-replay comparisons can provide `SIGNING_CERT_SOURCE_DIR=<path>`. This is a maintainer-oriented option documented by `make help-dev`; normal firmware builds do not need it.

## How do I choose EDK2/CIX/Radxa source versions?

A firmware variant selects the combination of EDK2, CIX, Radxa, and local project sources to build. Use `make help-variants` to list the configured combinations.

```bash
make help-variants
```

Use the `RELEASE` variable to select one of those combinations. The documented form is the prefixless variant name shown by `make help-variants`:

```bash
make buildbox-firmware-build \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/local \
  FIRMWARE_BOARD=O6N \
  FIRMWARE_TARGET=RELEASE
```

The full internal branch name is also accepted:

```bash
make buildbox-firmware-build \
  RELEASE=source/release/custom/edk2-202602/cix-1.2/radxa-1.2.1/local \
  FIRMWARE_BOARD=O6N
```

Build targets render or reuse a cached detached worktree. They do not normally create or advance a named `source/release/**` branch, and those branches are treated as disposable caches rather than required source data. If you intentionally want a persistent materialised branch for development, inspection, or CI, set `PERSIST=1` with `make render-release-branch`:

```bash
make render-release-branch \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/local-1.2.1 \
  PERSIST=1
```

This creates or verifies:

```text
source/release/custom/edk2-202602/cix-1.2/radxa-1.2.1/local-1.2.1
```

If an explicit local import changes the rendered tree, rebuild and replace the persistent branch deliberately:

```bash
make render-release-branch \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/local \
  PERSIST=1 REBUILD=1 FORCE=1
```

That command also refreshes the variant tree-ID metadata in `config/refs/variant-tree_id.json`.

A configured build variation is considered supported only when all of its source inputs are recorded locally. At a high level, this means:

- the selected EDK2, `edk2-platforms`, and `edk2-non-osi` base refs are present
- the Radxa vendor changes are available for that EDK2 release
- any selected CIX component refs are present under `source/component/cix/<cix-release>/`
- the compatible local project source checkpoint and local delta are present
- `source/base/edk2/edk2/edk2-stable*` refs declare the supported EDK2 release set from `202208` onward
- the chosen Radxa, CIX, and local source layers are present as repo refs
- `config/refs/*.json` records the source and rendered tree IDs needed to verify it
- the build policy records the relevant distro defaults and replay availability

Run this to check that the derived build matrix and available refs agree:

```bash
make verify-build-matrix
```

## How are source layers represented?

The firmware tree is built from several layers rather than from one permanent monolithic branch.

`source/base/**` branches contain upstream sources, such as EDK2, TF-A, and OP-TEE, at recorded versions. For EDK2 releases, the separate upstream `edk2`, `edk2-platforms`, and `edk2-non-osi` repos are combined into generated `source/base/rendered/**` skeleton caches when needed. Those cache refs are not required source data when `config/refs/base-tree_id.json` and the referenced component refs are present.

`source/component/cix/**` branches contain CIX-provided components. CIX publishes OP-TEE under a `tee` directory, but this source model records that component as `op-tee`.

`source/delta/radxa/**` branches contain Radxa's changes relative to a selected EDK2 base. `source/delta/local/**` branches contain this project's changes relative to a selected rendered vendor baseline.

A delta branch is not a full source tree. It contains:

- `metadata.json` with the base and target object IDs plus submodule metadata
- `delta.patch`, generated with `git diff --binary --full-index`
- `README.md` describing the artefact

This representation is intentional: a patch can record deletions, renames, binary files, and intentionally absent files in a way that a plain directory tree cannot. Render plans are derived from the selected variant name and available source refs, then verified against the ref metadata in `config/refs/`.

Render plans can also include an explicit `materialise_submodules` step. If any gitlinks remain after all configured steps have run, the renderer attempts recursive submodule materialisation automatically using the nearest recorded `.gitmodules` mapping and writes a submodule report under `.cache/edk2-cix/reports/`. That report is mainly a diagnostic and audit aid: it records which submodule paths were flattened, which commit IDs were used, which URL was recorded for each submodule, and which `.gitmodules` file supplied that mapping. You can usually ignore it when a build succeeds, but it is useful when checking that a rendered branch contains ordinary files rather than active submodules.

`source/release/**` branches are materialised firmware branches. They are generated from the base, component, vendor, and local layers. They are convenient to inspect or build, but they are not required source inputs: this repository treats them as caches, and a checkout may omit them and regenerate the selected variant from the recorded source layers and manifests.

## How do I start developing on this codebase?

1. Choose the firmware variant you want to develop against with `make help-variants`.
2. Materialise that firmware variant.
3. Create a normal topic branch from the materialised firmware branch.
4. Build and test your change.
5. Import the finished change back through `source/unofficial/current` with `make import-local-commits`.

Example:

```bash
make render-release-branch \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/local \
  PERSIST=1
git switch -c my-change source/release/custom/edk2-202602/cix-1.2/radxa-1.2.1/local
```

If you later import that topic branch back into the source model, also materialise the matching vendor baseline because `make import-local-commits` compares your topic branch against that baseline:

```bash
make render-release-branch \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1 \
  PERSIST=1
```

The supported workflows guard `source/base/**`, non-local `source/delta/**`, and `source/component/cix/**` refs by comparing them against the expected object IDs and tree IDs in `config/refs/*.json`. Git itself does not make those branch names immutable, so do not edit or move them by hand. If a guarded ref moved unexpectedly, or is checked out in a dirty worktree, the scripts abort before using it. Only `make integrate-source-release` should create or advance those refs.

## How do I persist development changes back to `source/unofficial/current`?

Local development is imported explicitly. Ordinary build and render targets never rewrite `source/unofficial/current` or generated `source/delta/local/*` artefacts.

The `BASE_REF` used below must exist locally. If generated `source/release/**` cache branches have been pruned, recreate the matching vendor baseline first with `make render-release-branch RELEASE=<vendor-variant> PERSIST=1`.

Dry run first:

```bash
make import-local-commits \
  BASE_REF=source/release/vendor/edk2-202602/cix-1.2/radxa-1.2.1 \
  FROM_REF=my-change
```

Then update the local source branch and generated local delta deliberately:

```bash
make import-local-commits \
  BASE_REF=source/release/vendor/edk2-202602/cix-1.2/radxa-1.2.1 \
  FROM_REF=my-change \
  UPDATE_LOCAL_SOURCE=1 \
  WRITE=1
```

The key variables are:

- `BASE_REF` is the rendered vendor baseline your topic branch is based on.
- `FROM_REF` is the topic branch or commit containing your finished change.
- `SOURCE_LOCAL_REF` is the full local source branch to update. It defaults to `source/unofficial/current` and must stay under `source/unofficial/`.
- `TARGET_REF` is the generated delta branch to write. It defaults to `source/delta/local/current` and must stay under `source/delta/local/`. This branch stores the patch that the renderer later applies to the selected baseline.

For each supported EDK2 release, the repo keeps two related records of the local project changes:

- a local source checkpoint, such as `source/unofficial/edk2-stable202602`, which is a normal source branch known to apply cleanly to that EDK2 release
- a generated local delta, such as `source/delta/local/edk2-stable202602`, which stores the patch from that release's vendor baseline to the matching local source checkpoint
- a local checkpoint tag, such as `source/unofficial/edk2/stable-202602`, which marks the commit known to apply to that EDK2 release without colliding with the branch name

Here, an EDK2 release means the upstream EDK2 code together with its matching `edk2-platforms` and `edk2-non-osi` companion sources. Some local changes apply unchanged across several EDK2 releases; others need small adjustments because upstream files moved or changed.

For example, if the same local source commit works on both `edk2-stable202502` and `edk2-stable202505`, both checkpoint tags may point at that commit. If `edk2-stable202508` needs an extra adjustment, make that adjustment at the `202508` checkpoint, then tag the adjusted commit as `source/unofficial/edk2/stable-202508`.

Checkpoint tags must remain reachable from retained `source/unofficial/**` branches, rather than becoming tag-only orphan commits. The tags deliberately use the non-colliding `source/unofficial/edk2/stable-*` namespace while the matching branches use `source/unofficial/edk2-stable*`.

## How do I update upstream EDK2, Arm TF-A, OP-TEE, CIX, or Radxa sources?

Use `make integrate-source-release`. Without `WRITE=1`, it validates arguments and prints the refs it would create.

Examples:

```bash
make integrate-source-release TYPE=upstream COMPONENT=edk2 RELEASE=edk2-stable202602
make integrate-source-release TYPE=upstream COMPONENT=tf-a RELEASE=v2.7
make integrate-source-release TYPE=vendor VENDOR=cix RELEASE=1.2
make integrate-source-release TYPE=vendor VENDOR=radxa RELEASE=1.2.1 EDK2_BASE=edk2-stable202208 REF=<local-ref>
```

When the dry run is correct, add `WRITE=1`. The same change updates `config/refs/*.json` with the new object IDs and tree IDs before being committed.

`TYPE=upstream` is for base components: `edk2`, `edk2-platforms`, `edk2-non-osi`, `tf-a`, or `op-tee`. `TYPE=vendor` is for a vendor integration target. `VENDOR=radxa` updates the Radxa EDK2 vendor layer. `VENDOR=cix` updates the CIX release bundle, whose TF-A and OP-TEE contents are tracked as separate internal components so future Arm-upstream uplifts remain possible.

The CIX bundle records original vendor provenance. To start an experimental uplift of one CIX component to a newer Arm upstream:

1. Integrate the Arm base you want to try:

   ```bash
   make integrate-source-release TYPE=upstream COMPONENT=tf-a RELEASE=v2.12 WRITE=1
   make integrate-source-release TYPE=upstream COMPONENT=op-tee RELEASE=4.4.0 WRITE=1
   ```

2. Create a normal topic branch from the selected Arm base and port the CIX component changes onto it.
3. When the port builds and has been reviewed, record the resulting component ref:

   ```bash
   make integrate-source-release \
     TYPE=vendor VENDOR=cix RELEASE=1.2 \
     COMPONENT=tf-a ARM_BASE=v2.12 \
     REF=<ported-tf-a-ref> WRITE=1
   ```

   The recorded ref is `source/component/cix/1.2/tf-a/v2.12`. Use `COMPONENT=op-tee ARM_BASE=4.4.0` for OP-TEE.

4. Add or update a firmware variant entry that selects the uplifted component ref, render it, and run the same build/audit qualification used for EDK2 releases.

The component port itself is intentionally a source-level engineering step rather than an automatic patch replay. The deterministic part starts once the reviewed component ref is recorded in `source/component/cix/**` and `config/refs/cix.json`.

Radxa non-release updates use the same vendor integration path. First update or fetch the vendor source branch into a local ref, then give the delta a release-like name that records the most recent release plus the vendor commit, for example:

```bash
make integrate-source-release \
  TYPE=vendor VENDOR=radxa \
  RELEASE=1.2.1+<short-commit> \
  EDK2_BASE=edk2-stable202208 \
  REF=main
```

For Radxa vendor refs, `MATERIALISE=1` is the default. This recursively flattens a submodule-shaped vendor source ref before the `source/delta/radxa/**` artefact is generated, so the delta describes ordinary file content rather than gitlinks.

After adding source refs for a new supported EDK2 release:

1. Integrate the required source refs with `make integrate-source-release`.

   At minimum this means the upstream `edk2` base ref and the selected companion `edk2-platforms` and `edk2-non-osi` refs. Once `source/base/edk2/edk2/<edk2-release>` exists, the release is discoverable; there is no separate release-list file to update.

2. Generate the variant once and refresh its recorded tree ID:

   ```bash
   make render-release-branch \
     RELEASE=edk2-202605/cix-1.2/radxa-1.2.1/local \
     PERSIST=1 REBUILD=1 FORCE=1
   ```

3. Run `make verify-build-matrix` to confirm the derived matrix, variant manifest, refs, aliases, and tree IDs agree.
4. Run the normal build/audit qualification for the new variant before publishing it.

The persistent `source/release/**` branch created in step 2 is a cache. Once the tree ID has been recorded and validation passes, it may be deleted without losing the ability to regenerate the variant.

`make help-variants` lists firmware variants derived from the available EDK2, Radxa, CIX, and local refs.

## How do I project `source/unofficial/current` to a materialised firmware branch?

Render a configured variant that includes the local layer:

```bash
make render-release-branch \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/local \
  PERSIST=1
```

Then validate it:

```bash
make verify-release-branch \
  RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/local
```

Materialised refs are generated mechanically from `source/base/**`, `source/component/**`, `source/delta/radxa/**`, and generated `source/delta/local/**` artefacts derived from `source/unofficial/current` compatibility points. They may be stored as `source/release/**` branches for convenience, but ordinary build and validation targets regenerate them when those branches are absent.

## How do I test a floating upstream tip instead of the latest release?

Integrate the upstream component using an explicit `REF` or a release-like name chosen for the experiment, then render a test variant branch that references it.

```bash
make integrate-source-release TYPE=upstream COMPONENT=edk2 REF=<upstream-object> RELEASE=floating-test WRITE=1
```

Floating-tip tests should use clearly named experimental refs and should not replace stable `source/base/**` refs until the release mapping and validation results are recorded in `config/refs/`.

## How do I work when an upstream remote is unavailable?

Ordinary builds do not require GitHub or any external upstream/vendor remote when the required source refs and manifests are present in this repository clone.

The fallback order is:

1. Use a local generated `source/release/**` cache if it exists.
2. Regenerate the selected variant from local source refs and manifests.
3. For source integration only, contact the external upstream/vendor remote if required objects are missing.
4. Fail only if required source data is unavailable locally and remotely.

There is no separate offline mode flag. The scripts try to proceed from local data first and warn or fail only when missing objects make the requested operation impossible.

## Validation checklist

For normal firmware building, the build target itself performs the necessary preflight checks. When changing source refs, variant manifests, or materialised branches, run:

```bash
make test
make lint
make verify-build-matrix
make verify-manifest-integrity
make check-identity-hygiene
make ref-report
```

For a materialised branch, also run:

```bash
make verify-release-branch RELEASE=edk2-202602/cix-1.2/radxa-1.2.1/local
```
