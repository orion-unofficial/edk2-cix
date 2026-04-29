# EDK2-CIX Reconstruction Control Branch

This branch contains the manifests, scripts, and top-level Makefile used to reconstruct buildable firmware branches from explicit source components:

- upstream bases such as EDK2, TF-A, and OP-TEE
- vendor layers from Radxa and CIX
- local project changes
- rendered release branches that are ready to build

The existing known-good branches remain available while this model is proven. The control branch is deliberately small: it should be safe to inspect, clone, and use as an orchestration entry point without checking out a full firmware tree first.

Rendered release branches must be ordinary Git trees. They must not retain active submodule gitlinks or active root `.gitmodules` files. `git log <path>` and `git blame <path>` must work from any directory inside the rendered checkout.

## How do I start developing on this codebase?

1. Clone the repository and switch to the control branch.
2. Run `make help` and `make help-vars`.
3. Render or check out the firmware release you want to develop against.
4. Develop on a normal topic branch created from the rendered release branch.
5. Persist the finished change back through `source/delta/local/current` with `make import-local-commits`.

Example:

```bash
git switch control
make help
make help-vars
make render-release-branch \
  RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local \
  PERSIST=1
git switch -c my-change source/release/custom/edk2-202602/cix-1.2/radxa-1.2.1/local
```

The `source/base/**`, non-local `source/delta/**`, and `source/component/cix/**` refs are immutable for normal development. They are only created or advanced through `make integrate-source-release`.

## How do I build the latest firmware?

The latest configured release is recorded in `config/releases.json`. Ordinary build targets render or reuse a cached worktree for that release and then delegate to the release branch Makefile.

```bash
git switch control
make build-all
```

Useful packaging targets are also delegated:

```bash
make install
make zip
make targz
```

Set `V=1` for verbose script and delegated build output:

```bash
make build-all V=1
```

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

## How do I persist development changes back to `source/delta/local/current`?

Local development is imported explicitly. Ordinary build and render targets never rewrite `source/delta/local/current`.

Dry run first:

```bash
make import-local-commits \
  BASE_REF=source/release/vendor/edk2-202602/cix-1.2/radxa-1.2.1 \
  FROM_REF=my-change
```

Then update the local delta ref:

```bash
make import-local-commits \
  BASE_REF=source/release/vendor/edk2-202602/cix-1.2/radxa-1.2.1 \
  FROM_REF=my-change \
  WRITE=1
```

`BASE_REF` is the rendered vendor baseline your topic branch is based on. `TARGET_REF` defaults to `source/delta/local/current` and must remain under `source/delta/local/`.


## How are deltas represented?

`source/delta/radxa/**` and `source/delta/local/current` are delta artifact branches. Each contains:

- `metadata.json` with base/target object IDs and submodule metadata
- `delta.patch` generated with `git diff --binary --full-index`
- `README.md` describing the artifact

This representation is intentional: a plain tree cannot encode deletions or renames relative to a base, while a binary patch can. Render plans in `config/releases.json` apply these artifacts in order.

Render plans can also include an explicit `materialize_submodules` step. If any gitlinks remain after all configured steps have run, the renderer attempts recursive submodule materialization automatically using the nearest recorded `.gitmodules` mapping and writes a submodule report under `.cache/edk2-cix/reports/`.

## How do I project `source/delta/local/current` to a materialized firmware branch?

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

Rendered refs are generated mechanically from `source/base/**`, `source/component/**`, `source/delta/radxa/**`, and `source/delta/local/current`. The older known-good branches remain as validation references until the reconstructed release branches are explicitly promoted.

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

Run these from the control branch:

```bash
make help
make help-vars
make render-release-branch-help
make integrate-source-release-help
make import-local-commits-help
make extract-vendor-delta-help
make verify-release-branch-help
make check-identity-hygiene
```

For a rendered branch, also run:

```bash
make verify-release-branch RELEASE=custom/edk2-202602/cix-1.2/radxa-1.2.1/local
```

A release branch is not considered fully reconstructed until it has no gitlinks, no active root `.gitmodules`, recursive submodule content is materialized as ordinary files, and history commands work from nested paths.
