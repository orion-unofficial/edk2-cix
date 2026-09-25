#!/usr/bin/env python3
"""Resumable, isolated custom-release construction and real build qualification."""

from __future__ import annotations

import argparse
import ast
from collections import Counter
import fcntl
import hashlib
import importlib.util
import itertools
import json
import os
from pathlib import Path
import re
import shutil
import shlex
import signal
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
RUNNERS = ("release_expansion.py", "release_expansion_source.py")
RADXA = ("1.2.1", "1.2.2", "1.2.3", "1.2.4", "1.3.0", "1.3.1")
BATCH_DEBUG_MASK = "0x80000001"


def save(path: Path, data: dict) -> None:
    pending = path.with_name(f".{path.name}.{os.getpid()}.{time.time_ns()}.new")
    pending.write_text(json.dumps(data, indent=2) + "\n")
    pending.replace(path)


def read(path: Path) -> dict:
    return json.loads(path.read_text()) if path.exists() else {}


def git(repo: Path, *args: str) -> str:
    return subprocess.check_output(["git", "-C", str(repo), *args], text=True).strip()


def refs(repo: Path) -> dict:
    return dict(line.split(" ", 1) for line in git(
        repo, "for-each-ref", "--format=%(refname) %(objectname)",
        "refs/heads", "refs/tags").splitlines()
                if not line.startswith("refs/heads/source/cache/"))


def event(state: Path, message: str) -> None:
    line = time.strftime("%Y-%m-%d %H:%M:%S ") + message
    print(line, flush=True)
    with (state / "progress.log").open("a") as out:
        out.write(line + "\n")


def selections(value: str, allowed: list[str]) -> list[str]:
    values = allowed if value == "all" else value.split(",")
    if not values or len(set(values)) != len(values) or set(values) - set(allowed):
        raise ValueError(f"expected all or unique comma-separated values from {allowed}: {value}")
    return values


def select_pair(plan: dict, value: str) -> dict:
    """Limit one invocation without changing the durable matrix or recipe."""
    parts = value.split("/")
    if (len(parts) != 2 or parts[0] not in plan["edk2"]
            or parts[1] not in plan["radxa"]):
        raise ValueError(f"--only-pair must be EDK2/RADXA from the frozen plan: {value}")
    return dict(plan, edk2=[parts[0]], radxa=[parts[1]])


def selected_pair_passed(state: Path, plan: dict, prepare_only: bool) -> bool:
    job = state / "jobs" / f"{plan['edk2'][0]}-{plan['radxa'][0]}"
    if read(job / "receipt.json").get("status") != "prepared":
        return False
    if prepare_only:
        return True
    return all(read(job / f"{board}-fixes-{fixes}-settings-{settings}" / "receipt.json")
               .get("status") == "passed"
               for board, fixes, settings in itertools.product(
                   plan["boards"], plan["fixes"], plan["settings"]))


def initialise(state: Path, args: argparse.Namespace) -> dict:
    if (state / "plan.json").exists():
        plan = read(state / "plan.json")
        for key in ("edk2", "radxa", "boards", "fixes", "settings"):
            requested = getattr(args, key)
            if requested != "all" and requested.split(",") != plan[key]:
                raise ValueError(f"--{key} differs from frozen plan; use a new --state")
        if args.platform and args.platform != plan["platform"]:
            raise ValueError("--platform differs from frozen plan; use a new --state")
        return plan
    # A failed initial clone must not be mistaken for a complete snapshot.
    if (state / "repo").exists():
        raise ValueError("incomplete initialization: retain this directory for review and use a new --state")
    if git(ROOT, "diff", "HEAD", "--name-only"):
        raise ValueError("commit tracked changes before taking a batch snapshot")
    from reconstruction_common import matrix_release_values
    edk2 = selections(args.edk2, matrix_release_values(ROOT))
    radxa = selections(args.radxa, list(RADXA))
    boards = selections(args.boards, ["O6", "O6N"])
    fixes = selections(args.fixes, ["false", "true"])
    settings = selections(args.settings, ["false", "true"])
    plan = {"schema": 1, "id": str(uuid.uuid4()), "origin": str(ROOT),
            "build_commit": git(ROOT, "rev-parse", "HEAD"), "refs": refs(ROOT),
            "edk2": edk2, "radxa": radxa, "boards": boards, "fixes": fixes,
            "settings": settings, "platform": args.platform or (
                "linux/arm64" if os.uname().machine in {"arm64", "aarch64"} else "linux/amd64"),
            "build_date": time.strftime("%Y-%m-%dT%H:%M:%S+00:00", time.gmtime()),
            "min_free_gib": args.min_free_gib}
    event(state, "Taking an isolated local repository snapshot; no remote publication")
    repo = state / "repo"
    subprocess.run(["git", "clone", "--local", "--no-checkout", "--no-tags", str(ROOT), str(repo)], check=True)
    # Local clones hard-link immutable Git objects, but have their own refs/index.
    git(repo, "checkout", "--detach", plan["build_commit"])
    git(repo, "fetch", "--no-tags", str(ROOT), "refs/heads/source/*:refs/heads/source/*",
        "refs/tags/*:refs/tags/*")
    copied = refs(repo)
    for ref, oid in plan["refs"].items():
        if ref.startswith(("refs/heads/source/", "refs/tags/")) and copied.get(ref) != oid:
            raise ValueError(f"input changed while snapshotting: {ref}")
    git(repo, "remote", "set-url", "--push", "origin", "disabled://release-expansion")
    git(repo, "config", "gc.auto", "0")
    for key in ("user.name", "user.email"):
        git(repo, "config", key, git(ROOT, "config", "--get", key))
    git(repo, "checkout", "-b", "batch-control")
    for name in RUNNERS:
        shutil.copy2(ROOT / "scripts" / name, repo / "scripts" / name)
    git(repo, "add", "--", *["scripts/" + name for name in RUNNERS])
    if git(repo, "diff", "--cached", "--name-only"):
        git(repo, "-c", "commit.gpgsign=false", "commit", "-m", "batch: snapshot release expansion runner")
    plan["runner_hashes"] = {name: hashlib.sha256((repo / "scripts" / name).read_bytes()).hexdigest()
                             for name in RUNNERS}
    # Guard only refs actually present in the private clone, including its build
    # branch and every tag. New candidates are permitted; old inputs never move.
    plan["protected_refs"] = {r: oid for r, oid in refs(repo).items() if r != "refs/heads/batch-control"}
    plan["policy_sha256"] = hashlib.sha256((repo / "config/policies.json").read_bytes()).hexdigest()
    save(state / "resolutions.json", {})
    save(state / "plan.json", plan)
    (state / "SCRATCH-PATHS.txt").write_text(
        f"Batch {plan['id']}\n{state}\n"
        "repo/: independent refs and resumable source candidates; never delete before review/import\n"
        "jobs/: logs, receipts, conflict notes and successful images\n"
        "tmp/: helper scratch; conflicts are retained as batch/conflicts refs and notes\n"
        "cache/: build downloads and the named Docker buildbox (see buildbox/buildbox-name)\n"
        "repo/.cache/: generated worktrees removed after each pair\n"
        "plan.json, progress.log, summary.json, resolutions.json, lock: resumable control state\n")
    return plan


def guard(state: Path, plan: dict) -> None:
    repo = state / "repo"
    current = refs(repo)
    for ref, oid in plan["protected_refs"].items():
        if current.get(ref) != oid:
            raise ValueError(f"protected input changed in batch repository: {ref}")
    if hashlib.sha256((repo / "config/policies.json").read_bytes()).hexdigest() != plan["policy_sha256"]:
        raise ValueError("batch changed current-release policy")
    for name, digest in plan["runner_hashes"].items():
        if hashlib.sha256((repo / "scripts" / name).read_bytes()).hexdigest() != digest:
            raise ValueError(f"runner changed during batch: {name}")
    if shutil.disk_usage(state).free < plan["min_free_gib"] * 1024**3:
        raise ValueError(f"less than {plan['min_free_gib']} GiB free; batch stopped for resumable disk cleanup")


def environment(state: Path, plan: dict) -> dict:
    # Do not inherit Make overrides or source-integration selectors from a shell.
    allowed = {"PATH", "HOME", "USER", "LOGNAME", "SHELL", "LANG", "TERM",
               "DOCKER_HOST", "DOCKER_CONTEXT", "DOCKER_CONFIG", "DOCKER_TLS_VERIFY",
               "DOCKER_CERT_PATH", "HTTP_PROXY", "HTTPS_PROXY", "NO_PROXY",
               "http_proxy", "https_proxy", "no_proxy", "SSH_AUTH_SOCK", "XDG_RUNTIME_DIR"}
    env = {k: v for k, v in os.environ.items() if k in allowed or k.startswith("LC_")}
    env.update(PYTHONDONTWRITEBYTECODE="1", PYTHONUNBUFFERED="1",
               TMPDIR=str(state / "tmp"), TMP=str(state / "tmp"), TEMP=str(state / "tmp"),
               EDK2_CIX_TMP_ROOT=str(state / "tmp"),
               EDK2_CIX_WORKTREE_NAMESPACE="batch-" + plan["id"])
    return env


def execute(state: Path, plan: dict, command: list[str], log: Path, label: str) -> int:
    """Stream full output to disk; print bounded elapsed/last-output updates."""
    event(state, f"START {label}; log={log.relative_to(state)}")
    started = time.monotonic()
    with log.open("w") as stream:
        child = subprocess.Popen(command, cwd=state / "repo", env=environment(state, plan),
                                 stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            while True:
                try:
                    code = child.wait(timeout=60)
                    break
                except subprocess.TimeoutExpired:
                    with log.open("rb") as latest:
                        latest.seek(max(0, log.stat().st_size - 1024))
                        lines = latest.read().decode(errors="replace").splitlines()
                    event(state, f"RUNNING {label}; {int(time.monotonic()-started)}s; "
                          + (lines[-1][-220:] if lines else "no output yet"))
        except KeyboardInterrupt:
            os.killpg(child.pid, signal.SIGINT)
            try:
                child.wait(timeout=20)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGTERM)
                child.wait()
            raise
    event(state, f"END {label}; exit={code}; {int(time.monotonic()-started)}s")
    return code


def archive_conflicts(state: Path, job: Path, existing: set[Path] | None = None) -> list[dict]:
    """Keep conflict commits/notes, not one multi-GB checkout for every conflict."""
    repo = state / "repo"
    result = []
    for notes in sorted((state / "tmp").glob("port-*-conflict-*/README.md")):
        if existing and notes in existing:
            continue
        wt = notes.parent / "worktree"
        if not wt.exists():
            continue
        if git(wt, "status", "--porcelain"):
            result.append({"worktree": str(wt), "reason": "dirty worktree retained"})
            continue
        oid = git(wt, "rev-parse", "HEAD")
        ref = f"refs/heads/batch/conflicts/{job.name}/{oid}"
        if ref not in refs(repo):
            git(repo, "update-ref", ref, oid, "0" * 40)
        elif refs(repo)[ref] != oid:
            raise ValueError(f"conflict archive ref changed: {ref}")
        destination = job / ("conflict-" + oid + ".md")
        original_notes = notes.read_text()
        command = shlex.join(["git", "-C", str(repo), "worktree", "add", "--detach",
                              str(state / ("review-" + job.name)), ref])
        destination.write_text(
            "# Archived conflict\n\nThe original checkout was removed after preserving its commit.\n"
            "Recreate a checkout with the command below; the original notes that follow\n"
            "refer to the old path. Use the resulting resolution commit in resolutions.json.\n\n"
            f"```bash\n{command}\n```\n\n" + original_notes)
        result.append({"ref": ref, "commit": oid, "notes": str(destination.relative_to(state)),
                       "paths": [line[4:] for line in original_notes.splitlines() if line.startswith("  - ")]})
        git(repo, "worktree", "remove", str(wt))
        shutil.rmtree(notes.parent)
    return result


def commit_metadata(repo: Path) -> None:
    git(repo, "add", "--", "config")
    if git(repo, "diff", "--cached", "--name-only"):
        git(repo, "-c", "commit.gpgsign=false", "commit", "-m", "batch: record prepared release candidates")


def build_command(plan: dict, edk2: str, radxa: str, board: str,
                  fixes: str, settings: str, state: Path, output: Path) -> list[str]:
    return ["make", "build", f"RELEASE=edk2-{edk2}/radxa-{radxa}/unofficial",
            "ARTEFACT_MODE=custom", f"FIRMWARE_BOARD={board}", "FIRMWARE_TARGET=RELEASE",
            "FIRMWARE_DISTRO=trixie", f"ENABLE_FIRMWARE_FIXES={fixes}", "ENABLE_CORE_ORDER=cix",
            f"ENABLE_EXPERIMENTAL_UEFI_SETTINGS={settings}", "DEBUG_VERBOSE=false", "CIX_RELEASE=",
            "FORCE_DEBUG_BUILD=0", f"DEBUG_PRINT_ERROR_LEVEL={BATCH_DEBUG_MASK}",
            f"BUILD_DATE={plan['build_date']}", f"BUILDBOX_PLATFORM={plan['platform']}",
            f"FIRMWARE_CACHE_ROOT={state / 'cache'}", f"BUILD_DIST_ROOT={output}"]


def verify_output(output: Path, board: str, fixes: str, settings: str, radxa: str) -> dict:
    def one(name: str) -> Path:
        matches = list(output.rglob(name))
        if len(matches) != 1:
            raise ValueError(f"expected one {name}, found {len(matches)}")
        return matches[0]
    image, options, chain, bl1 = (one(n) for n in (
        "cix_flash_all.bin", "BuildOptions", "firmware-chain-validation.json", "bootloader1-validation.json"))
    if image.stat().st_size != 8 * 1024**2:
        raise ValueError("full flash image is not exactly 8 MiB")
    digest = hashlib.sha256(image.read_bytes()).hexdigest()
    chain_data, bl1_data = read(chain), read(bl1)
    if chain_data.get("status") != "verified" or not any(
            i.get("image_sha256") == digest for i in chain_data.get("images", [])):
        raise ValueError("certificate-chain report does not verify this image")
    if bl1_data.get("acceptance_basis") not in {
            "approved-vendor-hash-and-signature", "approved-vendor-hash-fallback"}:
        raise ValueError("BL1 lacks approved vendor provenance")
    text = options.read_text()
    if "gCommandLineDefines: " not in text:
        raise ValueError("BuildOptions has no command-line defines")
    try:
        defines = ast.literal_eval(text.split("gCommandLineDefines: ", 1)[1].splitlines()[0])
    except (SyntaxError, ValueError, IndexError) as error:
        raise ValueError("BuildOptions has invalid command-line defines") from error
    if not isinstance(defines, dict):
        raise ValueError("BuildOptions command-line defines must be a dictionary")
    suffix = ("+fixes" if fixes == "true" else "") + ("+experimental" if settings == "true" else "")
    # firmware_layout.py includes every explicit mask in the display version,
    # even with DEBUG_VERBOSE=false. The mask itself also replaces +custom
    # when neither optional feature is enabled.
    suffix += "+mask" + BATCH_DEBUG_MASK.removeprefix("0x")
    for name, expected in {"ENABLE_FIRMWARE_FIXES": fixes.upper(),
                           "ENABLE_EXPERIMENTAL_UEFI_SETTINGS": settings.upper(),
                           "DEBUG_VERBOSE": "FALSE", "DEBUG_PRINT_ERROR_LEVEL": BATCH_DEBUG_MASK,
                           "UEFI_FW_VERSION": radxa + suffix}.items():
        if defines.get(name) != expected:
            raise ValueError(f"BuildOptions mismatch: {name}={defines.get(name)!r}, expected {expected!r}")
    if f"/{board}/{board}.dsc" not in text:
        raise ValueError("BuildOptions board mismatch")
    return {"sha256": digest, "size": image.stat().st_size,
            "image": str(image.relative_to(output)), "defines": defines,
            "validator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}


def frozen_runner(state: Path, plan: dict):
    """Keep the original source/build recipe; allow audited controller repairs."""
    guard(state, plan)
    path = state / "repo/scripts/release_expansion.py"
    spec = importlib.util.spec_from_file_location("release_expansion_snapshot", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.verify_output = verify_output
    module.cleanup_rendered = cleanup_rendered
    return module


def record_validator(state: Path, plan: dict) -> None:
    path = state / "validator-history.json"
    history = read(path) or {"revisions": []}
    digest = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    revisions = history["revisions"]
    if not revisions or revisions[-1]["sha256"] != digest:
        revisions.append({"sha256": digest, "path": str(Path(__file__).resolve()),
                          "time": time.time(), "frozen_runner_sha256": plan["runner_hashes"]["release_expansion.py"]})
        save(path, history)
        event(state, f"VALIDATOR {digest}; original source/build runner remains frozen")


def revalidate(state: Path, plan: dict, runner) -> int:
    """Recover only completed builds rejected by the old version expectation."""
    recovered, rejected = 0, 0
    for edk2, radxa, board, fixes, settings in itertools.product(
            plan["edk2"], plan["radxa"], plan["boards"], plan["fixes"], plan["settings"]):
        job = state / "jobs" / f"{edk2}-{radxa}"
        case = job / f"{board}-fixes-{fixes}-settings-{settings}"
        path = case / "receipt.json"
        row = read(path)
        if not (row.get("status") == "failed" and row.get("returncode") == 0
                and row.get("error", "").startswith("BuildOptions mismatch: UEFI_FW_VERSION=")):
            continue
        guard(state, plan)
        try:
            source_receipt = read(job / "receipt.json")
            source = source_receipt.get("source", {})
            if source_receipt.get("status") != "prepared" or row.get("source") != source:
                raise ValueError("build receipt no longer matches prepared source")
            if git(state / "repo", "rev-parse", source["source_ref"]) != source["source_commit"]:
                raise ValueError("prepared source ref changed")
            expected = runner.build_command(plan, edk2, radxa, board, fixes, settings, state, case / "output")
            if row.get("command") != expected:
                raise ValueError("build receipt command differs from frozen recipe")
            artifact = verify_output(case / "output", board, fixes, settings, radxa)
        except ValueError as error:
            rejected += 1
            event(state, f"REVALIDATION FAILED {case.relative_to(state)}: {error}")
            continue
        # Keep the old failure as evidence; do not rebuild or rewrite firmware.
        archive = case / ("validation-attempt-" + str(time.time_ns()))
        archive.mkdir()
        save(archive / "receipt.json", row)
        row.update(status="passed", artifact=artifact, revalidated=time.time(),
                   previous_receipt=str((archive / "receipt.json").relative_to(state)))
        row.pop("error", None)
        save(path, row)
        recovered += 1
        event(state, f"REVALIDATED {case.relative_to(state)}: passed; existing image unchanged")
    status(state, details=False)
    event(state, f"REVALIDATION COMPLETE recovered={recovered}, rejected={rejected}; other failures unchanged")
    return 1 if rejected else 0


def cleanup_rendered(state: Path, plan: dict) -> None:
    from render_release_branch import cached_worktree_is_dirty

    repo = state / "repo"
    parent = repo / ".cache/edk2-cix/worktrees" / ("batch-" + plan["id"])
    for section in git(repo, "worktree", "list", "--porcelain").split("\n\n"):
        line = next((entry[9:] for entry in section.splitlines() if entry.startswith("worktree ")), "")
        if line and Path(line).is_relative_to(parent):
            wt = Path(line)
            # An imported CRLF blob can be reported modified solely because
            # .gitattributes requests LF on checkout. Compare raw bytes and
            # mode with the index before discarding a rendered checkout.
            if cached_worktree_is_dirty(wt):
                raise ValueError(f"rendered worktree has real changes; retained: {wt}")
            retire_buildbox_mount(state, wt)
            git(repo, "worktree", "remove", "--force", str(wt))


def retire_buildbox_mount(state: Path, worktree: Path) -> None:
    """Retire a batch-owned container before removing its bind-mounted checkout."""
    name_file = state / "cache/buildbox/buildbox-name"
    if not name_file.exists():
        return
    name = name_file.read_text().strip()
    if not re.fullmatch(r"edk2-cix-buildbox-[0-9a-f]{8}", name):
        raise ValueError(f"invalid batch buildbox name in {name_file}")
    inspected = subprocess.run(["docker", "inspect", name], capture_output=True, text=True)
    if inspected.returncode:
        if "no such object" in inspected.stderr.lower() or "no such container" in inspected.stderr.lower():
            return
        raise ValueError(f"cannot inspect batch buildbox {name}: {inspected.stderr.strip()}")
    try:
        mounts = json.loads(inspected.stdout)[0]["Mounts"]
        sources = {entry["Source"] for entry in mounts if entry["Type"] == "bind"}
    except (IndexError, KeyError, TypeError, ValueError) as error:
        raise ValueError(f"invalid Docker mount report for {name}") from error
    if str(worktree) not in sources:
        return
    if str(state / "cache/buildbox") not in sources:
        raise ValueError(f"buildbox {name} mounts the worktree without this batch's cache")
    removed = subprocess.run(["docker", "rm", "-f", name], capture_output=True, text=True)
    if removed.returncode:
        raise ValueError(f"cannot retire batch buildbox {name}: {removed.stderr.strip()}")
    event(state, f"RETIRED buildbox {name} before removing rendered worktree {worktree}")


def status(state: Path, details: bool = True) -> dict:
    plan = read(state / "plan.json")
    jobs = state / "jobs"
    receipts = [dict(read(p), receipt=str(p.relative_to(state)))
                for p in sorted(jobs.glob("**/receipt.json"))
                if p.parent.parent == jobs or (
                    p.parent.parent.parent == jobs and p.parent.name.startswith(("O6-", "O6N-")))]
    counts = dict(Counter(r.get("status", "unknown") for r in receipts))
    pairs = len(plan.get("edk2", [])) * len(plan.get("radxa", []))
    builds = pairs * len(plan.get("boards", [])) * len(plan.get("fixes", [])) * len(plan.get("settings", []))
    summary = {"id": plan.get("id"), "planned_sources": pairs, "planned_builds": builds,
               "counts": counts, "receipts": receipts}
    print(f"Plan: {pairs} source pairs, {builds} builds; passed={counts.get('passed', 0)}/{builds}", flush=True)
    save(state / "summary.json", summary)
    print("Status: " + ", ".join(f"{k}={v}" for k, v in sorted(counts.items())), flush=True)
    for row in receipts:
        if details and row.get("status") in {"failed", "blocked", "running", "interrupted"}:
            print(f"  {row['status']}: {row['receipt']} {row.get('error', '')}", flush=True)
    return summary


def run(state: Path, plan: dict, prepare_only: bool, retry: bool) -> int:
    repo = state / "repo"
    pairs = list(itertools.product(plan["edk2"], plan["radxa"]))
    builds = list(itertools.product(plan["boards"], plan["fixes"], plan["settings"]))
    event(state, f"PLAN {len(pairs)} source pairs, {len(pairs)*len(builds)} builds; prepare_only={prepare_only}")
    for index, (edk2, radxa) in enumerate(pairs, 1):
        guard(state, plan)
        job = state / "jobs" / f"{edk2}-{radxa}"
        job.mkdir(parents=True, exist_ok=True)
        receipt_path = job / "receipt.json"
        receipt = read(receipt_path)
        if receipt.get("status") in {"blocked", "interrupted", "running"} and not retry:
            event(state, f"SKIP {index}/{len(pairs)} {job.name}: prior problem; use --retry-failed after review")
            continue
        if receipt.get("status") != "prepared":
            if receipt:
                archive = job / ("prepare-attempt-" + str(time.time_ns()))
                archive.mkdir()
                for name in ("prepare.log", "receipt.json", "source.json",
                             "verify_source_lifecycle.py.log", "verify_release_branch.py.log"):
                    if (job / name).exists():
                        shutil.move(str(job / name), str(archive / name))
            receipt = {"status": "running", "started": time.time()}
            save(receipt_path, receipt)
            command = [sys.executable, str(repo / "scripts/release_expansion_source.py"),
                       "--state", str(state), "--edk2", edk2, "--radxa", radxa,
                       "--result", str(job / "source.json")]
            existing_conflicts = set((state / "tmp").glob("port-*-conflict-*/README.md"))
            code = execute(state, plan, command, job / "prepare.log", f"prepare {index}/{len(pairs)} {job.name}")
            receipt.update(status="prepared" if code == 0 else "blocked", returncode=code)
            receipt["conflicts"] = archive_conflicts(state, job, existing_conflicts)
            if code:
                paths = [p for item in receipt["conflicts"] for p in item.get("paths", [])]
                receipt["error"] = ("source conflict: " + ", ".join(paths[:5]) if paths else
                                    "source preparation failed; see prepare.log")
            guard(state, plan)
            commit_metadata(repo)
            if code == 0:
                receipt["source"] = read(job / "source.json")
                for script, extra in [
                    ("verify_source_lifecycle.py", ["--from-ref", receipt["source"]["source_ref"],
                                                    "--target-ref", receipt["source"]["source_ref"]]),
                ]:
                    code = execute(state, plan, [sys.executable, str(repo / "scripts" / script), *extra],
                                   job / (script + ".log"), script + " " + job.name)
                    if code:
                        receipt.update(status="blocked", returncode=code, error=script + " failed")
                        break
            save(receipt_path, receipt)
        if receipt["status"] != "prepared":
            event(state, f"BLOCKED {job.name}; see {job / 'prepare.log'}; continuing independent pairs")
            continue
        source = receipt["source"]
        if git(repo, "rev-parse", source["source_ref"]) != source["source_commit"]:
            raise ValueError(f"prepared source changed: {source['source_ref']}")
        if prepare_only:
            continue
        for board, fixes, settings in builds:
            guard(state, plan)
            if git(repo, "rev-parse", source["source_ref"]) != source["source_commit"]:
                raise ValueError(f"prepared source changed: {source['source_ref']}")
            case = job / f"{board}-fixes-{fixes}-settings-{settings}"
            case.mkdir(exist_ok=True)
            path = case / "receipt.json"
            old = read(path)
            if old.get("status") == "passed":
                actual = verify_output(case / "output", board, fixes, settings, radxa)
                if actual["sha256"] != old["artifact"]["sha256"] or old["source"] != source:
                    raise ValueError(f"successful receipt no longer matches inputs/output: {path}")
                continue
            if old and not retry:
                event(state, f"SKIP {case.relative_to(state)}: prior problem; use --retry-failed")
                continue
            if old:
                archived = case / ("attempt-" + str(time.time_ns()))
                archived.mkdir()
                for name in ("output", "receipt.json", "build.log"):
                    if (case / name).exists():
                        shutil.move(str(case / name), str(archived / name))
            output = case / "output"
            command = build_command(plan, edk2, radxa, board, fixes, settings, state, output)
            row = {"status": "running", "command": command, "source": source,
                   "build_commit": git(repo, "rev-parse", "HEAD"), "started": time.time()}
            save(path, row)
            try:
                code = execute(state, plan, command, case / "build.log", f"build {index}/{len(pairs)} {case.name} {job.name}")
                row.update(returncode=code, status="failed")
                if git(repo, "rev-parse", source["source_ref"]) != source["source_commit"]:
                    raise ValueError("source ref changed during compilation")
                if code == 0:
                    row["artifact"] = verify_output(output, board, fixes, settings, radxa)
                    row["status"] = "passed"
            except KeyboardInterrupt:
                row["status"] = "interrupted"
                save(path, row)
                raise
            except ValueError as error:
                row["error"] = str(error)
            row["seconds"] = time.time() - row["started"]
            save(path, row)
            event(state, f"RESULT {job.name}/{case.name}: {row['status']} {row.get('error', '')}")
            # Keep the tested image, metadata and reports; discard redundant
            # archives and extracted duplicate images only after qualification.
            if row["status"] == "passed":
                for file in output.rglob("*"):
                    if file.is_file() and file.name not in {"cix_flash_all.bin", "BuildOptions"} and file.suffix != ".json":
                        file.unlink()
            status(state, details=False)
        cleanup_rendered(state, plan)
    summary = status(state)
    bad = any(summary["counts"].get(s) for s in ("blocked", "failed", "running", "interrupted"))
    event(state, "FINISHED WITH ISSUES; review summary.json" if bad else "FINISHED; requested stages passed")
    return 1 if bad else 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("action", choices=["run", "prepare", "status", "revalidate"])
    p.add_argument("--state", required=True, type=Path, help="Durable session-labelled batch directory; options freeze on first use")
    p.add_argument("--edk2", default="all")
    p.add_argument("--radxa", default="all")
    p.add_argument("--boards", default="all")
    p.add_argument("--fixes", default="all")
    p.add_argument("--settings", default="all")
    p.add_argument("--platform", choices=["linux/arm64", "linux/amd64"])
    p.add_argument("--min-free-gib", type=int, default=12)
    p.add_argument("--retry-failed", action="store_true")
    p.add_argument("--only-pair", help="Run or prepare only one EDK2/Radxa pair from the frozen plan")
    args = p.parse_args()
    if args.only_pair and args.action not in {"run", "prepare"}:
        p.error("--only-pair applies only to run or prepare")
    if args.min_free_gib < 1:
        p.error("--min-free-gib must be positive")
    state = args.state.resolve()
    if args.only_pair and not (state / "plan.json").exists():
        p.error("--only-pair requires an existing frozen batch plan")
    if args.action == "revalidate" and not (state / "plan.json").exists():
        p.error("no batch plan at --state")
    if args.action == "status":
        if not (state / "plan.json").exists():
            p.error("no batch plan at --state")
        status(state)
        return 0
    state.mkdir(parents=True, exist_ok=True)
    (state / "tmp").mkdir(exist_ok=True)
    with (state / "lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            p.error("this batch is already running")
        plan = initialise(state, args)
        runner = frozen_runner(state, plan)
        record_validator(state, plan)
        if args.action == "revalidate":
            return revalidate(state, plan, runner)
        selected = select_pair(plan, args.only_pair) if args.only_pair else plan
        result = runner.run(state, selected, args.action == "prepare", args.retry_failed)
        if args.only_pair:
            return 0 if selected_pair_passed(state, selected, args.action == "prepare") else 1
        return result


if __name__ == "__main__":
    def interrupted(_signum, _frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, interrupted)
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("Interrupted; state retained. Review status, then resume with --retry-failed.", file=sys.stderr)
        raise SystemExit(130)
    except (ValueError, subprocess.CalledProcessError) as error:
        print(f"Batch stopped: {error}", file=sys.stderr)
        raise SystemExit(2)
