#!/usr/bin/env bash

set -euo pipefail

is_usable_temp_root() {
    local candidate="${1%/}"

    [[ -n "$candidate" ]] || return 1
    [[ -d "$candidate" ]] || return 1
    [[ -w "$candidate" ]] || return 1
    [[ -x "$candidate" ]] || return 1
}

for candidate in \
    "${TMPDIR:-}" \
    "${TMP:-}" \
    "${TEMP:-}" \
    "${TEMPDIR:-}" \
    "${XDG_RUNTIME_DIR:-}" \
    /var/tmp \
    /tmp; do
    if is_usable_temp_root "$candidate"; then
        printf '%s\n' "${candidate%/}"
        exit 0
    fi
done

printf '%s\n' /tmp
