# EDK2-CIX Firmware Build

This branch contains the Makefile, manifests, and scripts used to select source versions and build firmware for the Radxa Orion O6 and O6N boards. The source model combines:

- upstream bases such as EDK2, TF-A, and OP-TEE
- Radxa and CIX vendor source layers
- local project changes
- materialised firmware branches that are ready to build

Most users only need the Makefile targets. Run:

```bash
make help
make help-vars
make help-releases
```

## How do I build the latest firmware?

The latest configured firmware release is recorded in `config/releases.json`. Ordinary build targets render or reuse a cached worktree for that release and then delegate to the release branch Makefile. User-facing build and packaging targets use the buildbox by default, so the host does not need an AArch64 firmware toolchain installed.

```bash
make build-all
```

Useful packaging targets are delegated through the same path:

```bash
make zip
make targz
```

`make install` builds and stages the payload first, then checks the selected install root before copying anything. By default it installs under `/boot/efi`; set `INSTALL_ROOT=/boot` or another path if your system uses a different mount point. Existing firmware payload files under `INSTALL_ROOT` are never replaced unless you rerun with `FORCE=1`.

```bash
make install
make install FORCE=1
```

Set `V=1` for verbose script and delegated build output:

```bash
make build-all V=1
```

Exact-replay comparisons can provide `SIGNING_CERT_SOURCE_DIR=<path>`. The wrapper copies the certificate inputs into the rendered worktree before invoking the buildbox, so the path remains available inside the container and does not dirty source-controlled files.

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

Radxa non-release updates use the same vendor integration path. First update or fetch the vendor source branch into a local ref, then give the delta a release-like name that records the most recent release plus the vendor commit, for example:

```bash
make integrate-source-release \
  TYPE=vendor VENDOR=radxa \
  RELEASE=1.2.1+<short-commit> \
  EDK2_BASE=edk2-stable202208 \
  REF=main
```

For Radxa vendor refs, `MATERIALISE=1` is the default. This recursively flattens a submodule-shaped vendor source ref before the `source/delta/radxa/**` artefact is generated, so the delta describes ordinary file content rather than gitlinks.

After adding source refs for a new supported release:

1. Add the release to `config/build-matrix.json` if it should be part of the supported matrix.
2. Add or update the corresponding render plans in `config/releases.json`.
3. Create the persistent release refs with `make render-release-branch RELEASE=<release> PERSIST=1`.
4. Run `make verify-build-matrix` to confirm the policy matrix, release manifest, refs, aliases, and tree IDs agree.
5. Run the normal build/audit qualification for the new release before publishing it.

`make help-releases` lists the currently configured releases directly from `config/releases.json`.

## How do I choose EDK2/CIX/Radxa source versions?

Use the `RELEASE` variable. The documented form is the short name without the `source/release/` prefix:

```bash
make build-all RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local
```

The full branch name is also accepted:

```bash
make build-all RELEASE=source/release/custom/edk2-202602/cix-1.2/radxa-1.2.1/local
```

To list configured releases:

```bash
make help-releases
```

To create or verify a persistent materialised branch, set `PERSIST=1`:

```bash
make render-release-branch \
  RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local-1.2.1 \
  PERSIST=1
```

This creates or verifies:

```text
source/release/custom/edk2-202602/cix-1.2/radxa-1.2.1/local-1.2.1
```

If `PERSIST=1` is not set, build targets still render or reuse a cached detached worktree, but they do not create or advance a named `source/release/**` branch. Use that mode for ordinary builds. Use `PERSIST=1` when you intentionally want a branch that other users or CI jobs can fetch and verify.

If an explicit local import changes the rendered tree, rebuild and replace the persistent release deliberately:

```bash
make render-release-branch \
  RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local \
  PERSIST=1 REBUILD=1 FORCE=1
```

That command also refreshes the rendered ref metadata in `config/refs/rendered.json` and the release tree ID in `config/releases.json`.

For a build variation to be fully pre-calculated from this repository, it needs all of the following recorded locally:

- `source/base/rendered/<edk2-release>` for the selected EDK2, `edk2-platforms`, and `edk2-non-osi` sources
- `source/delta/radxa/<radxa-release>/<edk2-release>` for the Radxa vendor layer carried to that EDK2 base
- any selected CIX component refs under `source/component/cix/<cix-release>/`
- a compatible local-source tag such as `source/unofficial/edk2-stable202602`
- a generated local delta artefact under `source/delta/local/<edk2-release>`
- a render plan in `config/releases.json`
- a build policy entry covering distro defaults and replay availability

If any of those inputs is missing, the build may still be possible, but that source combination has not yet been fully recorded as supported. Run:

```bash
make verify-build-matrix
```

That target compares `config/build-matrix.json` against `config/releases.json`, local `source/release/**` branches, `source/base/rendered/**` refs, `source/delta/radxa/**` artefacts, local compatibility tags, and alias tree IDs. A declared build variation is not supported until this check passes.

## How do I start developing on this codebase?

1. Choose the firmware release you want to develop against with `make help-releases`.
2. Render or check out that firmware release.
3. Develop on a normal topic branch created from the materialised release branch.
4. Persist the finished change back through `source/unofficial/current` with `make import-local-commits`.

Example:

```bash
make render-release-branch \
  RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local \
  PERSIST=1
git switch -c my-change source/release/custom/edk2-202602/cix-1.2/radxa-1.2.1/local
```

The supported workflows guard `source/base/**`, non-local `source/delta/**`, and `source/component/cix/**` refs by comparing them against the expected object IDs and tree IDs in `config/refs/*.json`. Git itself does not make those branch names immutable, so do not edit or move them by hand. If a guarded ref moved unexpectedly, or is checked out in a dirty worktree, the scripts abort before using it. Only `make integrate-source-release` should create or advance those refs.

## How do I persist development changes back to `source/unofficial/current`?

Local development is imported explicitly. Ordinary build and render targets never rewrite `source/unofficial/current` or generated `source/delta/local/*` artefacts.

Dry run first:

```bash
make import-local-commits \
  BASE_REF=source/release/vendor/edk2-202602/cix-1.2/radxa-1.2.1 \
  FROM_REF=my-change
```

Then update the local source branch and generated local delta artefact deliberately:

```bash
make import-local-commits \
  BASE_REF=source/release/vendor/edk2-202602/cix-1.2/radxa-1.2.1 \
  FROM_REF=my-change \
  UPDATE_LOCAL_SOURCE=1 \
  WRITE=1
```

`BASE_REF` is the rendered vendor baseline your topic branch is based on. `SOURCE_LOCAL_REF` defaults to `source/unofficial/current` and must remain under `source/unofficial/`. `TARGET_REF` defaults to `source/delta/local/current` and must remain under `source/delta/local/`.

For release-by-release EDK2 compatibility, keep `source/unofficial/current` as the human-readable latest local source branch and generate one local delta artefact per compatible EDK2 base, for example `source/delta/local/edk2-stable202208` or `source/delta/local/edk2-stable202602`. Compatibility tags such as `source/unofficial/edk2-stable202208` are full source checkpoints that overlay a given EDK2 release cleanly. If the same local source commit applies to multiple releases, multiple compatibility tags may point at it; if a release requires maintenance, commit the maintenance at the first affected release and tag the resulting compatible commit.

## How are source layers represented?

`source/unofficial/current` is an ordinary source branch. `source/delta/radxa/**` and `source/delta/local/**` are generated delta artefact branches. Each delta artefact contains:

- `metadata.json` with base/target object IDs and submodule metadata
- `delta.patch` generated with `git diff --binary --full-index`
- `README.md` describing the artefact

This representation is intentional: a plain tree cannot encode deletions or renames relative to a base, while a binary patch can. Render plans in `config/releases.json` apply these artefacts in order.

Render plans can also include an explicit `materialise_submodules` step. If any gitlinks remain after all configured steps have run, the renderer attempts recursive submodule materialisation automatically using the nearest recorded `.gitmodules` mapping and writes a submodule report under `.cache/edk2-cix/reports/`.

## How do I project `source/unofficial/current` to a materialised firmware branch?

Render a configured release that includes the local layer:

```bash
make render-release-branch \
  RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local \
  PERSIST=1
```

Then validate it:

```bash
make verify-release-branch \
  RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local
```

Materialised refs are generated mechanically from `source/base/**`, `source/component/**`, `source/delta/radxa/**`, and generated `source/delta/local/**` artefacts derived from `source/unofficial/current` compatibility points.

## How do I test a floating upstream tip instead of the latest release?

Integrate the upstream component using an explicit `REF` or a release-like name chosen for the experiment, then render a test release branch that references it.

```bash
make integrate-source-release TYPE=upstream COMPONENT=edk2 REF=<upstream-object> RELEASE=floating-test WRITE=1
```

Floating-tip tests should use clearly named experimental refs and should not replace stable `source/base/**` refs until the release mapping and validation results are recorded in `config/refs/`.

## How do I work when an upstream remote is unavailable?

Ordinary builds do not require external upstream/vendor remotes when the required materialised branch and source objects are already present locally or available from this repository origin.

The fallback order is:

1. Use the local materialised release ref.
2. Fetch the materialised release ref from this repository origin.
3. For source integration only, contact the external upstream/vendor remote if required objects are missing.
4. Fail only if required data is unavailable locally and remotely.

There is no separate offline mode flag. The scripts try to proceed from local data first and warn or fail only when missing objects make the requested operation impossible.

## Validation checklist

Run:

```bash
make verify-build-matrix
make check-identity-hygiene
```

For a materialised branch, also run:

```bash
make verify-release-branch RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local
```
