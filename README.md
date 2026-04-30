# EDK2-CIX Reconstruction Build Branch

This branch contains the manifests, scripts, and top-level Makefile used to reconstruct buildable firmware branches from explicit source components:

- upstream bases such as EDK2, TF-A, and OP-TEE
- vendor layers from Radxa and CIX
- local project changes
- rendered release branches that are ready to build

The existing known-good branches remain available while this model is proven. The build branch is deliberately small: it should be safe to inspect, clone, and use as an orchestration entry point without checking out a full firmware tree first.

Rendered release branches must be ordinary Git trees. They must not retain active submodule gitlinks or active root `.gitmodules` files. `git log <path>` and `git blame <path>` must work from any directory inside the rendered checkout.

## How do I start developing on this codebase?

1. Clone the repository and switch to the build branch.
2. Run `make help` and `make help-vars`.
3. Render or check out the firmware release you want to develop against.
4. Develop on a normal topic branch created from the rendered release branch.
5. Persist the finished change back through `source/unofficial/current` with `make import-local-commits`.

Example:

```bash
git switch build
make help
make help-vars
make render-release-branch \
  RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local \
  PERSIST=1
git switch -c my-change source/release/custom/edk2-202602/cix-1.2/radxa-1.2.1/local
```

The `source/base/**`, non-local `source/delta/**`, and `source/component/cix/**` refs are immutable for normal development. They are only created or advanced through `make integrate-source-release`.

## How do I build the latest firmware?

The latest configured release is recorded in `config/releases.json`. Ordinary build targets render or reuse a cached worktree for that release and then delegate to the release branch Makefile. User-facing build and packaging targets use the buildbox by default so the host does not need an AArch64 firmware toolchain installed.

```bash
git switch build
make build-all
```

Useful packaging targets are also delegated through the buildbox. `make install` builds and stages the payload first, then checks the selected install root before copying anything. By default it installs under `/boot/efi`; set `INSTALL_ROOT=/boot` or another path if your system uses a different mount point. Existing files are never replaced unless you rerun with `FORCE=1`.

```bash
make install
make install FORCE=1
make zip
make targz
```

Set `V=1` for verbose script and delegated build output:

```bash
make build-all V=1
```

Exact-replay comparisons can provide `SIGNING_CERT_SOURCE_DIR=<path>` from the
top-level `build` branch. The wrapper copies the certificate inputs into the
rendered worktree before invoking the buildbox, so the path remains available
inside the container and does not dirty source-controlled files.

## How do I choose EDK2/CIX/Radxa source versions?

Use the `RELEASE` variable. The documented form is the short name without the `source/release/` prefix:

```bash
make build-all RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local
```

The full branch name is also accepted:

```bash
make build-all RELEASE=source/release/custom/edk2-202602/cix-1.2/radxa-1.2.1/local
```

To create a persistent rendered branch, set `PERSIST=1`:

```bash
make render-release-branch \
  RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local-1.2.1 \
  PERSIST=1
```

This creates or verifies:

```text
source/release/custom/edk2-202602/cix-1.2/radxa-1.2.1/local-1.2.1
```

If an explicit local import changes the rendered tree, rebuild and replace the persistent release deliberately:

```bash
make render-release-branch \
  RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local \
  PERSIST=1 REBUILD=1 FORCE=1
```

That command also refreshes the rendered ref metadata in `config/refs/rendered.json` and the release tree ID in `config/releases.json`.

For a build variation to be judgement-free and reproducible from this repository, it needs all of the following recorded locally:

- `source/base/rendered/<edk2-release>` for the selected EDK2, `edk2-platforms`, and `edk2-non-osi` sources
- `source/delta/radxa/<radxa-release>/<edk2-release>` for the Radxa vendor layer carried to that EDK2 base
- any selected CIX component refs under `source/component/cix/<cix-release>/`
- a compatible local-source tag such as `source/unofficial/edk2-stable202208`
- a generated local delta artefact under `source/delta/local/<edk2-release>` for the CIX/Radxa matrix, or a more specific non-conflicting name such as `source/delta/local/edk2-stable202208-radxa-1.2.1` when two variants share the same EDK2 release but use different bases
- a render plan in `config/releases.json`
- a build policy entry covering distro defaults and replay availability

If any of those inputs is missing, the build may still be possible, but it has not yet been reduced to a fully pre-calculated source combination.

The supported source-tree combinations are explicitly enumerated in `config/build-matrix.json`. Use this check after adding a release, alias, or delta artefact:

```bash
make verify-build-matrix
```

That target compares the policy matrix against `config/releases.json`, local `source/release/**` branches, `source/base/rendered/**` refs, `source/delta/radxa/**` artefacts, and local compatibility tags. A declared build variation is not considered supported until this check passes.

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

For release-by-release EDK2 compatibility, keep `source/unofficial/current` as the human-readable latest local source branch and generate one local delta artefact per compatible EDK2 base, for example `source/delta/local/edk2-stable202208` or `source/delta/local/edk2-stable202602`. Compatibility tags such as `source/unofficial/edk2-stable202208` are full source checkpoints that overlay a given EDK2 release cleanly; they must not be semantic aliases to legacy branches. If the same local commit applies to multiple releases, multiple compatibility tags may point at it; if a release requires maintenance, commit the maintenance at the first affected release and tag that new compatible commit.


## How are deltas represented?

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

Rendered refs are generated mechanically from `source/base/**`, `source/component/**`, `source/delta/radxa/**`, and generated `source/delta/local/**` artefacts derived from `source/unofficial/current` compatibility points. The older known-good branches remain as validation references only; generated source refs and delta artefacts must not depend on those legacy branch names.

## How do I update upstream EDK2, Arm TF-A, OP-TEE, CIX, or Radxa sources?

Use `make integrate-source-release`. Without `WRITE=1`, it validates arguments and prints the refs it would create.

Examples:

```bash
make integrate-source-release TYPE=upstream COMPONENT=edk2 RELEASE=edk2-stable202602
make integrate-source-release TYPE=upstream COMPONENT=tf-a RELEASE=v2.7
make integrate-source-release TYPE=vendor VENDOR=cix RELEASE=1.2
make integrate-source-release TYPE=vendor VENDOR=radxa RELEASE=1.2.1 EDK2_BASE=edk2-stable202208 REF=<local-ref>
```

When the dry run is correct, add `WRITE=1`. The same change must update `config/refs/*.json` with the new object IDs and tree IDs before being committed.

CIX releases are treated as acquisition bundles from `https://github.com/cixtech/bios/`, but TF-A and OP-TEE remain separate internal components. That keeps future Arm-upstream uplift work possible.

Radxa non-release updates use the same vendor integration path. First update or fetch the vendor source branch into a local ref, then give the delta a release-like name that records the most recent release plus the vendor commit, for example:

```bash
make integrate-source-release \
  TYPE=vendor VENDOR=radxa \
  RELEASE=1.2.1+<short-commit> \
  EDK2_BASE=edk2-stable202208 \
  REF=main
```

For Radxa vendor refs, `MATERIALISE=1` is the default. This means a legacy submodule-shaped source ref such as `main` is recursively flattened before the `source/delta/radxa/**` artefact is generated, so the delta describes ordinary file content rather than gitlinks.

## How do I test a floating upstream tip instead of the latest release?

Integrate the upstream component using an explicit `REF` or a release-like name chosen for the experiment, then render a test release branch that references it.

```bash
make integrate-source-release TYPE=upstream COMPONENT=edk2 REF=<upstream-object> RELEASE=floating-test WRITE=1
```

Floating-tip tests should use clearly named experimental refs and should not replace stable source/base refs until the release mapping and validation results are recorded in `config/refs/`.

## How do I work when an upstream remote is unavailable?

Ordinary builds do not require external upstream/vendor remotes when the required rendered branch and source objects are already present locally or available from this repository origin.

The fallback order is:

1. Use the local rendered release ref.
2. Fetch the rendered release ref from this repository origin.
3. For integration/reconstruction only, contact the external upstream/vendor remote if required objects are missing.
4. Fail only if required data is unavailable locally and remotely.

There is no separate offline mode flag. The scripts try to proceed from local data first and warn or fail only when missing objects make the requested operation impossible.

## Validation checklist

Run these from the build branch:

```bash
make help
make help-vars
make render-release-branch-help
make integrate-source-release-help
make import-local-commits-help
make extract-vendor-delta-help
make verify-release-branch-help
make verify-build-matrix-help
make verify-build-matrix
make check-identity-hygiene
```

For a rendered branch, also run:

```bash
make verify-release-branch RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local
```

A release branch is not considered fully reconstructed until it has no gitlinks, no active root `.gitmodules`, recursive submodule content is materialised as ordinary files, and history commands work from nested paths.
