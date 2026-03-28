#!/usr/bin/env bash

set -euo pipefail

script_dir="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
src_dir="$(dirname -- "$script_dir")"
repo_root="$(dirname -- "$src_dir")"
upstream_ref="${UPSTREAM_EDK2_REF:-refs/heads/main-monorepo-upstream-edk2}"

usage() {
    echo "usage: $0 {source-commit|source-commit-short|source-date-epoch|source-date-iso}" >&2
    exit 2
}

git_repo_usable() {
    git -C "$repo_root" rev-parse --is-inside-work-tree >/dev/null 2>&1
}

resolve_anchor_commit() {
    local anchor_commit

    if ! git_repo_usable; then
        return 1
    fi

    anchor_commit="$(git -C "$repo_root" merge-base HEAD "$upstream_ref" 2>/dev/null || true)"
    if [[ -n "$anchor_commit" ]]; then
        printf '%s\n' "$anchor_commit"
        return
    fi

    git -C "$repo_root" rev-parse HEAD
}

resolve_source_commit() {
    if [[ -n "${SOURCE_COMMIT:-}" ]]; then
        printf '%s\n' "${SOURCE_COMMIT}"
        return
    fi
    if [[ -n "${SOURCE_COMMIT_HASH:-}" ]]; then
        printf '%s\n' "${SOURCE_COMMIT_HASH}"
        return
    fi

    resolve_anchor_commit
}

format_epoch_as_iso() {
    python3 - "$1" <<'PY'
from datetime import datetime, timezone
import sys

print(datetime.fromtimestamp(int(sys.argv[1]), timezone.utc).isoformat(timespec="seconds"))
PY
}

command="${1:-}"
case "$command" in
    source-commit)
        resolve_source_commit
        ;;
    source-commit-short)
        if [[ -n "${SOURCE_COMMIT_HASH:-}" ]]; then
            printf '%s\n' "${SOURCE_COMMIT_HASH}"
        else
            resolve_source_commit | cut -c1-10
        fi
        ;;
    source-date-epoch)
        if [[ -n "${SOURCE_DATE_EPOCH:-}" ]]; then
            printf '%s\n' "${SOURCE_DATE_EPOCH}"
        else
            git -C "$repo_root" show -s --format=%ct "$(resolve_anchor_commit)"
        fi
        ;;
    source-date-iso)
        if [[ -n "${SOURCE_DATE_EPOCH:-}" ]]; then
            format_epoch_as_iso "${SOURCE_DATE_EPOCH}"
        elif [[ -n "${BUILD_DATE:-}" ]]; then
            printf '%s\n' "${BUILD_DATE}"
        else
            git -C "$repo_root" show -s --format=%cI "$(resolve_anchor_commit)"
        fi
        ;;
    *)
        usage
        ;;
esac
