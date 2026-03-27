#!/usr/bin/env bash

set -euo pipefail

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
src_dir="$(dirname -- "$script_dir")"
repo_root="$(dirname -- "$src_dir")"

usage() {
    echo "usage: $0 {source-commit|source-commit-short|source-date-epoch|source-date-iso}" >&2
    exit 2
}

extract_trailer() {
    local commit="$1"
    local key="$2"

    git -C "$repo_root" show -s --format=%B "$commit" | sed -n "s/^${key}: //p" | head -n 1
}

resolve_anchor_commit() {
    local mapped_commit

    mapped_commit="$(git -C "$repo_root" log --format=%H -n 1 --all-match \
        --grep='^Origin-Repo: radxa-pkg/edk2-cix$' \
        --grep='^Origin-Commit: ' \
        HEAD 2>/dev/null || true)"
    if [[ -n "$mapped_commit" ]]; then
        printf '%s\n' "$mapped_commit"
        return
    fi

    git -C "$repo_root" rev-parse HEAD
}

resolve_source_commit() {
    local anchor_commit
    local source_commit

    anchor_commit="$(resolve_anchor_commit)"
    source_commit="$(extract_trailer "$anchor_commit" 'Origin-Commit')"
    if [[ -n "$source_commit" ]]; then
        printf '%s\n' "$source_commit"
        return
    fi

    printf '%s\n' "$anchor_commit"
}

format_epoch_as_iso() {
    python3 - "$1" <<'PY'
from datetime import datetime, timezone
import sys

print(datetime.fromtimestamp(int(sys.argv[1]), timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"))
PY
}

command="${1:-}"
case "$command" in
    source-commit)
        resolve_source_commit
        ;;
    source-commit-short)
        resolve_source_commit | cut -c1-10
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
