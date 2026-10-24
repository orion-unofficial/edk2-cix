# Release-matched StandaloneMM Actions preparation

The manually dispatched `stmm-release-matched.yaml` workflow prepares independent
release-native StandaloneMM cases. It currently supports input preflight and a
native compilation adapter. Full firmware, emulator and storage qualification
remain on their existing execution paths until their portability blockers are
resolved. A green preparation run does not qualify firmware or change production
support guards. Existing firmware workflows remain required.

`config/stmm-ci.json` is the consumed input/acceptance manifest. Its four initial
Radxa 1.3.1 cases are a bounded pilot, not a declaration of complete supported
release coverage. Eligibility continues to come from the source manifests and
policies. Each source and component has an exact commit/tree pin. Maintained
selection must match the live source ref and `refs-unofficial.json`; a moved ref
fails until reviewed and repinned. Candidate selection uses isolated objects and
never falls back to a maintained tip. EDK2, platform and non-OSI trees, native DSC,
component, reviewed CryptoPkg blob, stage-specific helper/test hashes and harness commit are
recorded in each result. Native libraries, GCC/GCC5 selection and vendor executable
hash checks remain in the existing native compiler helper.

The manifest distinguishes these input states:

- `qualified-candidate`: an isolated candidate passed its specified historical
  offline tests with pinned inputs. It needs a new attempt if inputs change.
- `integrated-unqualified`: equivalent candidate changes have been reviewed and
  incorporated in a maintained descendant, whose complete acceptance is pending.
- `qualified-descendant`: that exact maintained descendant and pinned inputs have
  independently passed every applicable required criterion.
- `not-integrated`: the selected maintained source still needs the candidate
  changes. It can be inspected but native compilation is blocked by this adapter.

Integration status requires focused source/patch-equivalence review; ancestry
alone does not establish it. Update pins and reviewed state in place after the
normal descendant integration workflow. No CI preflight or native compile emits
`qualified-descendant`. Results distinguish the input's state from this attempt's
`passed`, `failed`, `blocked`, `unverified` or `incomplete` criteria and always retain
`qualification: incomplete` in this preparation workflow. Every full qualification
must bind the exact source/tree/dependencies, helper/test-suite identities, image
and firmware hashes to reproducible evidence for every required criterion.

## First approved dispatch

Publish the infrastructure as an ordinary descendant of `build`, independently
of the unintegrated firmware topics. The minimum change is these five files:
`.github/workflows/stmm-release-matched.yaml`, `config/stmm-ci.json`,
`scripts/stmm_ci.py`, `scripts/test_stmm_ci.py` and this document. An optional
procedure link in `standalone-mm.md` can accompany them; exclude its other WIP.
Input-only preflight uses the standard library and Git, and consumes no native
helper modules, local images, session evidence or unpublished candidate objects.
Native compilation separately needs `qualify_stmm_releases.py`,
`stmm_release_platform.py`, `stmm_release_port.py`, `stmm_vendor_control.py` and
the native platform/vendor tests recorded by the adapter. Review those files and
their tracked import dependencies before enabling that lane. Keep the compiler
image unset until its prerequisites are satisfied.

The original failures in run `39028460047`, job `117384260914`, were repaired
in build ancestry. Baseline run `39953414390` passed source-model tests, lint,
required diagnostics and minimised-clone validation. This does not cover a new
publication revision or any StandaloneMM WIP/candidate/descendant. Before an
approved push, run the existing source-model, lint, source/identity/helper policy
and minimised export/clone gates against the isolated publication snapshot in an
adequately provisioned Linux environment. Preserve the normal publication checks
and commit identity requirements. A preflight success cannot waive those gates.
Publication requires separate authorization.

GitHub requires the dispatchable workflow to exist on the default branch, which
is `build`. Publishing it only on a topic branch cannot bootstrap dispatch.
The [manual workflow documentation](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow)
describes the default-branch requirement and branch selection. The
[dispatch API](https://docs.github.com/en/rest/actions/workflows#create-a-workflow-dispatch-event)
accepts a branch or tag ref; the UI branch selector does not select an arbitrary
commit SHA. For the first run, publish the complete infrastructure commit to
`refs/heads/build`, then use `--ref build` with its explicitly reviewed full SHA:

```bash
PUBLISHED_HARNESS_SHA='<full 40-character SHA of the approved published commit>'
test "$(gh api repos/orion-unofficial/edk2-cix/git/ref/heads/build --jq .object.sha)" = "$PUBLISHED_HARNESS_SHA"
gh workflow run stmm-release-matched.yaml --repo orion-unofficial/edk2-cix \
  --ref build -f selection=maintained -f stage=preflight \
  -f harness_sha="$PUBLISHED_HARNESS_SHA"
```

These are post-publication instructions, not authorization to execute them.
There is no publication SHA until the isolated change has been committed and
reviewed; local HEAD or a qualified firmware candidate cannot supply it.
If `build` advances before dispatch resolves the ref, the run must fail the SHA
guard. Each job checks out `github.sha` explicitly and requires the checked-out
commit, `github.workflow_sha` and `harness_sha` to agree. The workflow revision and
harness revision are thus the same commit. Firmware subjects remain the separate
exact source/component pins in `config/stmm-ci.json`; fetched branch names are
only availability and movement checks. Moved maintained refs require review and
repinning, never automatic selection of their new tips.

The first run evaluates maintained input identities and leaves full acceptance
outstanding. Inspect all four per-case artifacts and the aggregate, including
`qualification: incomplete`, before approving another stage. Publishing the five
files requires no source-ref changes, candidate-object publication or new tags.
The existing build-branch CI still classifies scripts/config/workflows as build
affecting and may start its normal firmware qualification jobs on an approved
push. This migration does not suppress or replace those jobs.

A candidate preflight additionally requires authorized immutable transfer of its
isolated firmware/component/OP-TEE objects. The workflow currently fetches only
published required source refs: objects missing from that fetch fail visibly.
Do not replace the candidate with whichever source happens to be available.
There is no 502 full-source candidate; candidate mode deliberately enumerates
only the three existing candidates. The maintained preflight includes 502 and
its full qualification remains outstanding.

For native compilation, first prepare a pullable Linux amd64 compiler image with
an immutable registry digest and record it as `compiler_image`. Local Docker image
IDs and local synthetic RepoDigests are insufficient. The existing Debian/apt
recipe is mutable; a newly built image is a new pinned attempt, not byte-identical
historical environment evidence. Verify toolchain/distro and record the digest,
compiler output and recipe provenance before approving a compile run. Maintained
native compilation also requires reviewed integration and refreshed source/helper
pins. The adapter applies only the existing native preparation and ports; it does
not render, import or integrate source, and does not build a complete firmware
image. A new output directory is required after any failed attempt or input change.

## Portability and evidence boundaries

Cases use separate Actions VMs, output directories and artifacts, with
`fail-fast: false`, `max-parallel: 2`, two compiler workers and finite timeouts.
There are no shared mutable worktrees, caches, timers or automatic retries.
Generic temporary variables point under a session-labelled workspace directory.
The native adapter passes explicit object IDs to the existing compiler, avoiding
its mutable manifest/default selector. Generated `/work` paths belong to each
new attempt; frozen historical `/Users` and `/work` plans are never rewritten.

The native lane requires at least 8 GiB free before and after image pull and
4 GiB available RAM. These are conservative entry guards, not measured full-build
capacity or proof a workload fits. Record actual runner disk, RAM, architecture
and duration before expanding. Standard hosted runners have limited local disk;
see [GitHub runner specifications](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
Use a separately sized runner if measurements require it; do not delete unrelated
runner data or relax checks to fit. Full-image/buildbox/QEMU capacity is unproven.

Historical emulator images are Linux arm64 local images, not verified public OCI
inputs; one historical 608 image is absent. Auth, C producer, cold persistence and
NOR storage need a portable supported caller with pinned image, fixtures, signed
input derivation and vendor controls before moving. Preserve native411/502 heap
failures and the 208-core boundary control's restricted storage-parity meaning.
The NOR acceptance still requires identical 2047 uninterrupted mutations/flash
bytes, then the same 189 ordinal cuts, including the 40 inherited failures and
149 recoveries. Counts alone cannot establish parity. No hardware evidence is
created by these jobs; O6 boot/UART/physical persistence and Secure Boot state
remain separately unverified and require explicit flash/reboot authorization.

Actions use published floating major-version tags, consistently across workflows,
so compatible upstream updates are adopted automatically. The exact harness and
firmware input identity checks remain required. Preflight does not install Python
packages or pull a compiler/emulator image. The hosted runner label remains a
managed environment; it does not establish historical compiler replay.

Artifacts contain exact input manifests, structured results, checksums, compiler
identity, logs, FD/profile and available ELF/map files. Failed attempts upload too.
The summary runs after failed/skipped matrix jobs, rejects missing, duplicate,
foreign or stale results and does not turn diagnostic failures into success.
The aggregate requires the input receipt and all stage-specific evidence, checks
its hashes, and rejects altered unevaluated criteria or missing dependency states.
The adapter rejects a reduced acceptance list or weakened resource entry guards.
Artifacts are retained for 30 days, subject to repository limits; download and
verify all evidence to durable storage before expiry. Full-image, signatures,
fixture/auth derivation and failed-cut artifacts must be added before those
workloads are enabled. Keep complete candidate/control pairs and ordinal plans
together. An artifact-upload failure is a failed workflow dependency.
