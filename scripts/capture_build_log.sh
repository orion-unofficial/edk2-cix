#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
warning_summary_script="${script_dir}/summarize_build_warnings.py"

usage() {
    cat >&2 <<'EOF'
usage: scripts/capture_build_log.sh <log-dir> <command> [args...]
EOF
}

if (( $# < 2 )); then
    usage
    exit 2
fi

log_dir="$1"
shift

mkdir -p "$log_dir"

timestamp="$(date -u +%Y%m%dT%H%M%SZ)"
base_name="build-${timestamp}"
log_file="${log_dir}/${base_name}.log"
summary_file="${log_dir}/${base_name}.summary.txt"
pipe_dir="$(mktemp -d "${TMPDIR:-/tmp}/capture-build-log.XXXXXX")"
pipe_path="${pipe_dir}/stream"
tee_pid=""

cleanup() {
    local status=$?
    trap - EXIT
    if [[ -n "${tee_pid:-}" ]]; then
        if kill -0 "$tee_pid" >/dev/null 2>&1; then
            kill "$tee_pid" >/dev/null 2>&1 || true
        fi
        wait "$tee_pid" 2>/dev/null || true
    fi
    rm -f "$pipe_path"
    rmdir "$pipe_dir" 2>/dev/null || true
    exit "$status"
}

trap cleanup EXIT
mkfifo "$pipe_path"

printf '[log] Command:'
printf ' %q' "$@"
printf '\n'
printf '[log] Writing full log to %s\n' "$log_file"

tee "$log_file" <"$pipe_path" &
tee_pid=$!

set +e
(
    exec </dev/null
    "$@" >"$pipe_path" 2>&1
)
cmd_status=$?
set -e

for _attempt in {1..20}; do
    if ! kill -0 "$tee_pid" >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

if kill -0 "$tee_pid" >/dev/null 2>&1; then
    printf '[log] Log capture helper did not exit promptly after the command finished; closing it.\n' >&2
    kill "$tee_pid" >/dev/null 2>&1 || true
fi
wait "$tee_pid" 2>/dev/null || true
tee_pid=""

{
    printf 'Command:'
    printf ' %q' "$@"
    printf '\n'
    printf 'Exit status: %s\n' "$cmd_status"
    printf 'Captured at: %s\n' "$timestamp"
    printf '\n'
    printf 'Warnings and errors:\n'
    if command -v rg >/dev/null 2>&1; then
        rg -n -i \
            -e '(^|[^[:alnum:]_-])warning:' \
            -e '^warning[[:space:]]+[0-9]+' \
            -e '(^|[^[:alnum:]_-])error:' \
            -e '^error[[:space:]]' \
            -e '(^|[^[:alpha:]])failed([:[:space:]]|$)' \
            -e '(^|[^[:alpha:]])missing([:[:space:]]|$)' \
            -e 'ambiguous upon' \
            -e 'LOAD segment with RWX permissions' \
            -e '^Traceback ' \
            "$log_file" || true
    else
        grep -Ein \
            'warning:|^warning[[:space:]][0-9]+|error:|^error[[:space:]]|failed|missing|ambiguous upon|LOAD segment with RWX permissions|^Traceback ' \
            "$log_file" || true
    fi
    printf '\n'
    if [[ -f "$warning_summary_script" ]]; then
        python3 "$warning_summary_script" "$log_file" || true
    fi
} >"$summary_file"

printf '[log] Summary written to %s\n' "$summary_file"
exit "$cmd_status"
