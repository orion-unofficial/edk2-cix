#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat >&2 <<'EOF'
usage: scripts/require_host_tools.sh <context> <command> [command...]
EOF
}

if (( $# < 2 )); then
    usage
    exit 2
fi

context="$1"
shift

missing=()
for tool in "$@"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        missing+=("$tool")
    fi
done

if (( ${#missing[@]} == 0 )); then
    exit 0
fi

host_desc="$(uname -srm)"
if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    if [[ -n "${PRETTY_NAME:-}" ]]; then
        host_desc="${PRETTY_NAME} (${host_desc})"
    fi
fi

cat >&2 <<EOF
[host-tools] Missing required host tool(s) for ${context}: ${missing[*]}
[host-tools] Host: ${host_desc}
[host-tools] This workflow depends on Debian packaging tools and cannot run correctly until those commands are available.
[host-tools] Use a Debian-based environment, install the required tools manually, or run the equivalent buildbox/devcontainer workflow instead.
EOF
exit 1
