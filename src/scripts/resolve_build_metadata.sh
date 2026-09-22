#!/usr/bin/env bash

set -euo pipefail

script_dir="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
src_dir="$(dirname -- "$script_dir")"
repo_root="$(dirname -- "$src_dir")"
upstream_ref="${UPSTREAM_EDK2_REF:-}"

usage() {
    echo "usage: $0 {source-commit|source-commit-short|source-date-epoch|source-date-iso|default-short-hash-length|abbreviate-commit <sha> [length]|component-commit <component>|component-commit-short <component> [length]|path-last-change -- <path>...|path-last-change-short [length] -- <path>...|profile-build-define <profile> <board> <key>|profile-build-define-length <profile> <board> <key>}" >&2
    exit 2
}

git_repo_usable() {
    git -C "$repo_root" rev-parse --is-inside-work-tree >/dev/null 2>&1
}

resolve_source_base_trailer() {
    if ! git_repo_usable; then
        return 1
    fi

    git -C "$repo_root" log -1 --format=%B HEAD 2>/dev/null | awk -F': ' '/^Source-Base: / { print $2; exit }'
}

resolve_anchor_commit() {
    local anchor_commit
    local base_ref

    if ! git_repo_usable; then
        return 1
    fi

    base_ref="$upstream_ref"
    if [[ -z "$base_ref" ]]; then
        base_ref="$(resolve_source_base_trailer || true)"
    fi

    if [[ -n "$base_ref" ]]; then
        anchor_commit="$(git -C "$repo_root" merge-base HEAD "$base_ref" 2>/dev/null || true)"
        if [[ -n "$anchor_commit" ]]; then
            printf '%s\n' "$anchor_commit"
            return
        fi
    fi

    git -C "$repo_root" rev-parse HEAD
}

resolve_source_commit() {
    if [[ -n "${SOURCE_COMMIT:-}" ]]; then
        printf '%s\n' "${SOURCE_COMMIT}"
        return
    fi

    resolve_anchor_commit
}

resolve_component_source_commit() {
    local component="${1:-}"
    local origin_repo
    local fallback_path
    local origin_commit

    case "$component" in
        edk2)
            if [[ -n "${EDK2_SOURCE_COMMIT:-}" ]]; then
                printf '%s\n' "${EDK2_SOURCE_COMMIT}"
                return
            fi
            origin_repo="radxa/edk2"
            fallback_path="src/edk2"
            ;;
        edk2-non-osi)
            if [[ -n "${EDK2_NON_OSI_SOURCE_COMMIT:-}" ]]; then
                printf '%s\n' "${EDK2_NON_OSI_SOURCE_COMMIT}"
                return
            fi
            origin_repo="radxa/edk2-non-osi"
            fallback_path="src/edk2-non-osi"
            ;;
        edk2-platforms)
            if [[ -n "${EDK2_PLATFORMS_SOURCE_COMMIT:-}" ]]; then
                printf '%s\n' "${EDK2_PLATFORMS_SOURCE_COMMIT}"
                return
            fi
            origin_repo="radxa/edk2-platforms"
            fallback_path="src/edk2-platforms"
            ;;
        *)
            return 1
            ;;
    esac

    if ! git_repo_usable; then
        return 1
    fi

    origin_commit="$(git -C "$repo_root" log HEAD -n 1 --format=%B \
        --grep="^Origin-Repo: ${origin_repo}$" \
        --all-match | sed -n 's/^Origin-Commit: //p' | head -n 1)"
    if [[ -n "$origin_commit" ]]; then
        printf '%s\n' "$origin_commit"
        return
    fi

    resolve_path_last_change "$fallback_path"
}

resolve_path_last_change() {
    if ! git_repo_usable; then
        return 1
    fi
    if [[ "$#" -eq 0 ]]; then
        return 1
    fi

    git -C "$repo_root" log HEAD -n 1 --format=%H -- "$@"
}

default_short_hash_length() {
    local anchor_commit
    local short_commit

    if ! git_repo_usable; then
        return 1
    fi

    anchor_commit="$(resolve_anchor_commit 2>/dev/null || true)"
    if [[ -z "$anchor_commit" ]]; then
        return 1
    fi

    short_commit="$(git -C "$repo_root" rev-parse --short "$anchor_commit" 2>/dev/null || true)"
    if [[ -z "$short_commit" ]]; then
        return 1
    fi

    printf '%s\n' "${#short_commit}"
}

abbreviate_commit() {
    local commit="${1:-}"
    local length="${2:-}"

    if [[ -z "$commit" ]]; then
        return 1
    fi

    if [[ -z "$length" ]]; then
        length="$(default_short_hash_length 2>/dev/null || true)"
    fi
    if [[ -z "$length" ]] || [[ ! "$length" =~ ^[0-9]+$ ]]; then
        return 1
    fi

    printf '%s\n' "${commit:0:length}"
}

resolve_profile_build_define() {
    local profile="${1:-}"
    local board="${2:-}"
    local key="${3:-}"

    if [[ -z "$profile" ]] || [[ -z "$board" ]] || [[ -z "$key" ]]; then
        return 1
    fi

    python3 - "$repo_root" "$profile" "$board" "$key" <<'PY'
import json
import pathlib
import sys

repo_root = pathlib.Path(sys.argv[1])
profile_name = sys.argv[2]
board = sys.argv[3]
key = sys.argv[4]
profile_file = repo_root / "validation" / "expected-hashes.json"

data = json.loads(profile_file.read_text(encoding="utf-8"))
profiles = data.get("profiles")
if not isinstance(profiles, dict):
    raise SystemExit(1)

profile = profiles.get(profile_name)
if not isinstance(profile, dict):
    raise SystemExit(1)

board_profiles = profile.get("boards")
if isinstance(board_profiles, dict):
    board_profile = board_profiles.get(board)
    if not isinstance(board_profile, dict):
        raise SystemExit(1)
else:
    board_profile = profile

build_options = board_profile.get("build_options")
if not isinstance(build_options, dict):
    raise SystemExit(1)

defines = build_options.get("gCommandLineDefines")
if not isinstance(defines, dict):
    raise SystemExit(1)

value = defines.get(key)
if not isinstance(value, str) or not value:
    raise SystemExit(1)

print(value)
PY
}

resolve_profile_build_define_length() {
    local value

    value="$(resolve_profile_build_define "${1:-}" "${2:-}" "${3:-}" 2>/dev/null || true)"
    if [[ -z "$value" ]]; then
        return 1
    fi

    printf '%s\n' "${#value}"
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
        abbreviate_commit "$(resolve_source_commit)"
        ;;
    default-short-hash-length)
        default_short_hash_length
        ;;
    abbreviate-commit)
        abbreviate_commit "${2:-}" "${3:-}"
        ;;
    component-commit)
        resolve_component_source_commit "${2:-}"
        ;;
    component-commit-short)
        abbreviate_commit "$(resolve_component_source_commit "${2:-}")" "${3:-}"
        ;;
    path-last-change)
        if [[ "${2:-}" != "--" ]]; then
            usage
        fi
        resolve_path_last_change "${@:3}"
        ;;
    path-last-change-short)
        if [[ "${2:-}" == "--" ]]; then
            abbreviate_commit "$(resolve_path_last_change "${@:3}")"
        elif [[ "${3:-}" == "--" ]]; then
            abbreviate_commit "$(resolve_path_last_change "${@:4}")" "${2:-}"
        else
            usage
        fi
        ;;
    profile-build-define)
        resolve_profile_build_define "${2:-}" "${3:-}" "${4:-}"
        ;;
    profile-build-define-length)
        resolve_profile_build_define_length "${2:-}" "${3:-}" "${4:-}"
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
