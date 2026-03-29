#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"
lock_root="${EDK2_CIX_BUILD_LOCK_ROOT:-${repo_root}/.buildbox/locks}"
lock_name="${EDK2_CIX_BUILD_LOCK_NAME:-repo-build}"
lock_dir="${lock_root}/${lock_name}.lock.d"
owner_file="${lock_dir}/owner"
wait_mode="${EDK2_CIX_BUILD_LOCK_WAIT:-0}"
break_stale="${EDK2_CIX_BUILD_LOCK_BREAK_STALE:-0}"
current_host="$(hostname 2>/dev/null || uname -n 2>/dev/null || printf 'unknown\n')"
current_user="${USER:-$(id -un 2>/dev/null || printf 'unknown\n')}"
current_pid="$$"
current_started="$(date -u '+%Y-%m-%dT%H:%M:%SZ' 2>/dev/null || date)"
current_cwd="$(pwd -P)"

usage() {
    cat >&2 <<'EOF'
usage: scripts/with_repo_lock.sh <command> [args...]
EOF
}

format_command() {
    local arg
    for arg in "$@"; do
        printf '%q ' "$arg"
    done
}

read_owner_value() {
    local key="$1"
    [[ -f "$owner_file" ]] || return 1
    sed -n "s/^${key}=//p" "$owner_file" | sed -n '1p'
}

describe_owner() {
    local owner_host owner_user owner_pid owner_started owner_cwd owner_command

    owner_host="$(read_owner_value host || true)"
    owner_user="$(read_owner_value user || true)"
    owner_pid="$(read_owner_value pid || true)"
    owner_started="$(read_owner_value started || true)"
    owner_cwd="$(read_owner_value cwd || true)"
    owner_command="$(read_owner_value command || true)"

    [[ -n "$owner_pid" ]] && printf '[lock] Held by PID %s' "$owner_pid" >&2 || printf '%s' '[lock] Held by an unknown owner' >&2
    [[ -n "$owner_user" ]] && printf ' as %s' "$owner_user" >&2
    [[ -n "$owner_host" ]] && printf ' on %s' "$owner_host" >&2
    printf '.\n' >&2
    [[ -n "$owner_started" ]] && printf '[lock] Started: %s\n' "$owner_started" >&2
    [[ -n "$owner_cwd" ]] && printf '[lock] CWD: %s\n' "$owner_cwd" >&2
    [[ -n "$owner_command" ]] && printf '[lock] Command: %s\n' "$owner_command" >&2
}

lock_is_stale() {
    local owner_host owner_pid

    owner_host="$(read_owner_value host || true)"
    owner_pid="$(read_owner_value pid || true)"

    [[ -n "$owner_pid" ]] || return 1
    [[ "$owner_host" == "$current_host" ]] || return 1
    ! kill -0 "$owner_pid" 2>/dev/null
}

cleanup() {
    local status="$?"
    local owner_pid

    owner_pid="$(read_owner_value pid || true)"
    if [[ -d "$lock_dir" && "$owner_pid" == "$current_pid" ]]; then
        rm -rf "$lock_dir"
    fi
    exit "$status"
}

case "${EDK2_CIX_REPO_LOCK_HELD:-0}" in
    1)
        exec "$@"
        ;;
esac

if (( $# == 0 )); then
    usage
    exit 2
fi

mkdir -p "$lock_root"

wait_seconds=0
case "$wait_mode" in
    ''|0|false|FALSE|no|NO)
        wait_seconds=0
        ;;
    1|true|TRUE|yes|YES)
        wait_seconds=-1
        ;;
    *)
        if [[ "$wait_mode" =~ ^[0-9]+$ ]]; then
            wait_seconds="$wait_mode"
        else
            printf '[lock] Unsupported EDK2_CIX_BUILD_LOCK_WAIT=%s; use 0, 1, or a number of seconds.\n' "$wait_mode" >&2
            exit 2
        fi
        ;;
esac

command_line="$(format_command "$@")"
command_line="${command_line% }"

write_owner_file() {
    cat >"$owner_file" <<EOF
pid=${current_pid}
user=${current_user}
host=${current_host}
started=${current_started}
cwd=${current_cwd}
command=${command_line}
EOF
}

start_epoch="$(date +%s 2>/dev/null || printf '0\n')"
announced_wait=0

while ! mkdir "$lock_dir" 2>/dev/null; do
    if [[ "$break_stale" == "1" ]] && lock_is_stale; then
        printf '[lock] Removing stale build lock left by PID %s on %s.\n' \
            "$(read_owner_value pid || printf 'unknown\n')" \
            "$(read_owner_value host || printf 'unknown\n')" >&2
        rm -rf "$lock_dir"
        continue
    fi

    if [[ "$announced_wait" -eq 0 ]]; then
        printf '[lock] Another build-related command is already using this checkout.\n' >&2
        describe_owner
        announced_wait=1
    fi

    if [[ "$wait_seconds" -eq 0 ]]; then
        printf '%s\n' '[lock] Refusing to run in parallel. Wait for the active build to finish, or set EDK2_CIX_BUILD_LOCK_WAIT=1 to wait.' >&2
        exit 1
    fi

    if [[ "$wait_seconds" -gt 0 ]]; then
        now_epoch="$(date +%s 2>/dev/null || printf '0\n')"
        if (( now_epoch - start_epoch >= wait_seconds )); then
            printf '[lock] Timed out waiting %ss for the active build lock.\n' "$wait_seconds" >&2
            exit 1
        fi
    fi

    sleep 1
done

write_owner_file
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

export EDK2_CIX_REPO_LOCK_HELD=1
"$@"
