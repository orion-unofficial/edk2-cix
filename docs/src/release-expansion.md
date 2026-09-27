# Unattended custom release expansion

`scripts/release_expansion.py` prepares custom source candidates and runs their
real public `make build` commands without an active agent. It never publishes
refs, changes the calling checkout's refs, or changes upstream build behaviour.

The default frozen plan covers all 18 recorded EDK2 releases from 202208 through
202608 (including 202408.01), six Radxa releases (1.2.1–1.2.4, 1.3.0, 1.3.1),
both boards, and both states of firmware fixes and experimental settings:
**108 source pairs and 864 builds**. Logging uses `DEBUG_VERBOSE=false` and the targeted INIT/error mask
`0x80000001`; verbose debug experiments are separate. O6 alone gives 432 builds.

## Start and monitor

Run from the build-branch checkout. Docker must be running, Git identity must be
configured, and Python 3.10 or newer is required. Commit tracked edits before starting. The runner records the input commit and
script hashes in its private snapshot. Use a durable directory
with at least 12 GiB free at each stage; the retained images alone can approach
7 GiB for the full matrix, with additional space needed for caches and sources.
Keep the computer awake for the run.

```bash
batch_dir="$PWD/.worktrees/release-expansion-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$batch_dir"
nohup python3 scripts/release_expansion.py run --state "$batch_dir" \
  > "$batch_dir/console.log" 2>&1 &
echo "$!" > "$batch_dir/runner.pid"
tail -f "$batch_dir/console.log"
```

For O6 only, add `--boards O6` to the **first** invocation. `--edk2`, `--radxa`,
`--fixes` and `--settings` also accept comma-separated subsets. The platform
defaults to native Linux containers for the host architecture; explicitly select
`--platform linux/amd64` or `--platform linux/arm64` on the first invocation if
needed. Initialization freezes the plan; use a new state directory for different
inputs. The private snapshot continues to use its original source-preparation
and build runner even if the calling checkout is subsequently updated. Output
verification and rendered-worktree cleanup use the invoking checkout's audited
controller, so fixes to those checks can apply without changing the frozen build
inputs or recipe. Its script hash is recorded in `validator-history.json` and
newly accepted artifacts.

Progress includes the pair index, current configuration, elapsed time, most
recent tool output, pass/failure totals and log location. A separate terminal can
show the compact status, including all outstanding failures:

```bash
python3 scripts/release_expansion.py status --state "$batch_dir"
```

`prepare` performs only source construction, provenance checks, overlay lifecycle
validation and source rendering. Run `run` with the same state later to compile.
The full build matrix is not started merely by requesting `status`.

## Results and resume

- `plan.json`: frozen inputs, original refs, selected axes and runner hashes.
- `progress.log`: concise append-only execution history.
- `summary.json`: current receipts and aggregate counts.
- `validator-history.json`: verifier revisions used with the frozen build runner.
- `jobs/<edk2>-<radxa>/prepare.log`: integration and source-validation details.
- `jobs/<pair>/<board>-fixes-<value>-settings-<value>/build.log`: complete build log.
- Each job has a `receipt.json`; successful build outputs retain the 8 MiB full
  flash image, `BuildOptions` and JSON validation/provenance reports.
- `repo/`: isolated repository containing new candidates, conflict refs and
  committed candidate metadata. **Retain it until the candidates are reviewed
  and imported or explicitly discarded.**

A failed source preparation blocks its pair; other independent pairs continue.
A failed build does not prevent the other configurations from running. No
signature, size, source-integrity or overlay checks are bypassed. A zero exit
from `make` is insufficient: the runner also verifies the output options, image
size, vendor BL1 acceptance and certificate report's binding to the image hash.
Compile/package success still does not establish successful booting on hardware
or prove that every automatically merged firmware change is semantically correct.
New candidates remain subject to review before promotion to maintained support.

One reviewed O6 DSDT CPU-reference conflict is resolved during preparation when
its 12 conflict groups, source inputs, stage and entire resulting file match
recorded identities. The private batch journals the resolution commit and
validates it again on resume. Any different DSDT input or other source conflict
still stops that pair for review.

Radxa 1.3 source preparation replays custom changes from the nearest valid
checkpoint of the same Radxa version while keeping vendor-port selection
independent. The batch runs the structural preflight before registering a
candidate. It catches cleanly merged overlay regressions such as references to
removed SMBIOS providers, incorrect CPU performance groups, stale O6 FDF
layouts, and missing PCIe SMMU build/menu hooks. It rejects release pairs
whose layout has not yet been reviewed. The check can also be run directly:

```bash
python3 scripts/validate_radxa13_source.py --repo . \
  --revision source/unofficial/1.3.1/edk2-stable202208 \
  --edk2 202208 --radxa 1.3.1
```

Rerunning `run` resumes unattempted work and verifies previously passed images
before skipping them. Failed/interrupted work stays visible without being
repeated automatically. After examining its logs or supplying a resolution:

```bash
python3 scripts/release_expansion.py run --state "$batch_dir" --retry-failed
```

Previous failed attempts are archived; a retry has a fresh output directory.
The runner has an exclusive process lock. Ctrl-C or SIGTERM stops its active
child process group and retains resumable state. A disk-space stop is resumable
after freeing space; it does not delete anything automatically to obtain space.
Exit status is 0 when the requested stages pass, 1 when cases need review, and
2 for a batch-level problem. Interruptions return 130.

To review and retry one pair before a wider retry, keep the frozen batch plan
and select just that pair:

```bash
python3 scripts/release_expansion.py run --state "$batch_dir" \
  --only-pair 202208/1.2.2 --retry-failed
```

The selected command succeeds when that pair's source and all selected builds
pass, even if other pairs still have failures. The global `summary.json` and
status output continue to show the complete batch.

### Recovering a version-verification failure

The batch explicitly passes `DEBUG_PRINT_ERROR_LEVEL=0x80000001`. Its version
therefore contains `+mask80000001` even with `DEBUG_VERBOSE=false`, for example
`1.2.4+fixes+experimental+mask80000001` or `1.2.4+mask80000001` with both features
disabled. Earlier runners incorrectly expected versions without this suffix.

After stopping the runner with Ctrl-C or SIGTERM, invoke the corrected script
from the build-branch checkout:

```bash
python3 scripts/release_expansion.py revalidate --state "$batch_dir"
python3 scripts/release_expansion.py run --state "$batch_dir"
```

`revalidate` recovers only completed builds whose `make` returned zero and which
failed the version check. It checks the frozen recipe and source, the exact
requested mask and feature flags, image size, BL1 acceptance and the certificate
report's image hash again. Original receipts are archived beside each recovered
case; firmware images, source refs and the frozen plan remain unchanged. It runs
no compilers. Genuine build failures and source conflicts remain visible and
need separate review; its exit status describes revalidation, not the whole batch.

An interrupted in-flight build still requires `--retry-failed` to compile again.
Recovered cases are verified and skipped on resume. The same exclusive lock
protects revalidation; do not run it concurrently with the batch or edit the
private runner/hash records to bypass that protection.

The cleanup check also compares any Git-reported modified rendered files with
their raw indexed bytes and executable modes. Some imported CRLF files appear
modified only because their historical bytes conflict with an `eol=lf`
attribute; byte-identical files can be removed as generated worktrees. Real
edits, deletions, staged changes and untracked files remain protected.
Before removing a rendered worktree or resuming a build, cleanup retires this
batch's Docker buildbox if it bind-mounts a batch worktree. The next build then
creates a fresh container against the current checkout rather than reusing a
stale mount.

## Conflict resolution and ref preservation

Preparation first reuses a valid exact checkpoint or baseline-compatible legacy
source. Missing tuples replay the nearest usable earlier Radxa delta on the same
EDK2 base; if necessary, the same Radxa release is ported from an earlier EDK2.
Existing exact checkpoints that fail provenance require review and are never
silently replaced. No current-line policy, original source ref or release tag
is advanced. New refs are recorded with their manifests using a recovery journal.

Conflicts are kept as `batch/conflicts/...` refs inside the private repository,
with their source-port notes in the pair's job directory. Their clean generated
checkouts are removed to avoid retaining a multi-gigabyte tree for every conflict.
The receipt gives the exact ref. Recreate a working tree when reviewing it:

```bash
git -C "$batch_dir/repo" worktree add --detach \
  "$batch_dir/review-conflict" refs/heads/batch/conflicts/<pair>/<commit>
```

After resolving the real source files and committing, put the resulting commit
in `resolutions.json`, for example:

```json
{
  "202211/1.2.2": {
    "unofficial_ref": "<resolved-commit>",
    "unofficial_stage": "auto"
  }
}
```

Vendor-port resolutions use `port_ref`. The unofficial stage can be `source`,
`overlay`, or `final` when the conflict notes require an explicit stage. Resolve
patch conflicts by reconstructing and mechanically regenerating the patch;
never hand-edit patch hunks. Then rerun with `--retry-failed`. The batch does not
guess conflict resolutions or need a person to babysit successful compiles.

## Storage and cleanup

The private clone hard-links Git objects where possible but has independent refs
and no Git alternates dependency. Generated build worktrees are removed after
each pair. Successful outputs discard duplicate archives while retaining the
tested image and reports. Caches, the named Docker buildbox, conflict notes,
failed outputs and private source candidates remain available for review.
`SCRATCH-PATHS.txt` records their locations. Do not prune the original source refs
or delete the private repository before preserving new work worth retaining.

## Build-policy boundaries

The runner uses normal RELEASE gating, retains the vendor trusted firmware and
does not enable enlarged layouts. See [BL33 sizing](debug-layout.md) when
qualifying verbose logging separately. Batch success produces reviewable source
candidates; it does not publish releases or promote support policy.
