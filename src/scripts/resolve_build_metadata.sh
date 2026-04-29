#!/usr/bin/env bash

set -euo pipefail

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
src_dir="$(dirname -- "$script_dir")"
repo_root="$(dirname -- "$src_dir")"
upstream_ref="${UPSTREAM_EDK2_REF:-refs/heads/main-monorepo-upstream-edk2}"

usage() {
    echo "usage: $0 {source-commit|source-commit-short|source-date-epoch|source-date-iso}" >&2
    exit 2
}

resolve_anchor_commit() {
    local anchor_commit

    anchor_commit="$(git -C "$repo_root" merge-base HEAD "$upstream_ref" 2>/dev/null || true)"
    if [[ -n "$anchor_commit" ]]; then
        printf '%s\n' "$anchor_commit"
        return
    fi

    git -C "$repo_root" rev-parse HEAD
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
        resolve_anchor_commit
        ;;
    source-commit-short)
        resolve_anchor_commit | cut -c1-10
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
        else
            git -C "$repo_root" show -s --format=%cI "$(resolve_anchor_commit)"
        fi
        ;;
    *)
        usage
        ;;
esac
