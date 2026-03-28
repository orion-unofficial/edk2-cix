#!/usr/bin/env bash

set -euo pipefail

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

printf '[log] Command:'
printf ' %q' "$@"
printf '\n'
printf '[log] Writing full log to %s\n' "$log_file"

set +e
"$@" 2>&1 | tee "$log_file"
cmd_status=${PIPESTATUS[0]}
set -e

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
    if [[ -f "./scripts/summarize_build_warnings.py" ]]; then
        python3 ./scripts/summarize_build_warnings.py "$log_file" || true
    fi
} >"$summary_file"

printf '[log] Summary written to %s\n' "$summary_file"
exit "$cmd_status"
