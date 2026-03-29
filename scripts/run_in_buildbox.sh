#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"
workspace_parent="${EDK2_CIX_WORKSPACE_PARENT:-/workspaces}"
workspace_path="${workspace_parent}/$(basename -- "$repo_root")"
host_workspace_parent="$(dirname -- "$repo_root")"
container_tmpdir="${EDK2_CIX_CONTAINER_TMPDIR:-/hosttmp}"
host_tmpdir="${EDK2_CIX_HOST_TMPDIR:-${TMPDIR:-/tmp}}"
host_tmpdir="${host_tmpdir%/}"
if [[ -z "$host_tmpdir" ]]; then
    host_tmpdir="/tmp"
fi

container_name="${EDK2_CIX_BUILDBOX_NAME:-edk2-cix-buildbox}"
container_image="${EDK2_CIX_BUILDBOX_IMAGE:-mcr.microsoft.com/devcontainers/base:bookworm}"
container_runtime="${EDK2_CIX_CONTAINER_RUNTIME:-}"

status() {
    printf '[buildbox] %s\n' "$*"
}

resolve_container_runtime() {
    if [[ -n "$container_runtime" ]]; then
        printf '%s\n' "$container_runtime"
        return 0
    fi

    local candidate
    for candidate in podman docker; do
        if command -v "$candidate" >/dev/null 2>&1 && "$candidate" info >/dev/null 2>&1; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    for candidate in docker podman; do
        if command -v "$candidate" >/dev/null 2>&1; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    cat >&2 <<'EOF'
[buildbox] Neither podman nor docker is available on PATH.
EOF
    exit 1
}

runtime() {
    "$container_runtime" "$@"
}

verify_workspace() {
    if ! runtime exec -w "$workspace_path" "$container_name" test -f Makefile; then
        cat >&2 <<EOF
[buildbox] Expected the repo root to be mounted at ${workspace_path}, but Makefile is missing there.
[buildbox] Check EDK2_CIX_WORKSPACE_PARENT, the host workspace bind mount, and the checkout path.
EOF
        exit 1
    fi

    if ! runtime exec -w "$workspace_path" "$container_name" test -f src/Makefile; then
        cat >&2 <<EOF
[buildbox] Expected src/Makefile inside ${workspace_path}, but it was not found.
[buildbox] This usually means the wrong host directory was mounted into the build container.
EOF
        exit 1
    fi
}

ensure_container() {
    if runtime container inspect "$container_name" >/dev/null 2>&1; then
        local mounts
        mounts="$(runtime inspect -f '{{range .Mounts}}{{printf "%s=%s\n" .Destination .Source}}{{end}}' "$container_name")"
        if ! grep -Fxq "${workspace_parent}=${host_workspace_parent}" <<<"$mounts" || \
            ! grep -Fxq "${container_tmpdir}=${host_tmpdir}" <<<"$mounts"; then
            status "Recreating ${container_name} with the expected workspace mounts"
            runtime rm -f "$container_name" >/dev/null
        elif [[ "$(runtime inspect -f '{{.State.Running}}' "$container_name")" != "true" ]]; then
            status "Starting existing container ${container_name}"
            runtime start "$container_name" >/dev/null
            return 0
        else
            return 0
        fi
    fi

    status "Creating container ${container_name}"
    runtime run -d \
        --name "$container_name" \
        --platform linux/amd64 \
        --ulimit nofile=1024:524288 \
        -v "${host_workspace_parent}:${workspace_parent}" \
        -v "${host_tmpdir}:${container_tmpdir}" \
        -w "$workspace_path" \
        "$container_image" \
        sleep infinity >/dev/null
}

if (( $# == 0 )); then
    cat >&2 <<'EOF'
usage: scripts/run_in_buildbox.sh <command> [args...]
EOF
    exit 2
fi

container_runtime="$(resolve_container_runtime)"
status "Using container runtime: ${container_runtime}"
ensure_container
mkdir -p "$host_tmpdir"
verify_workspace

status "Ensuring build dependencies are present"
runtime exec -w "$workspace_path" "$container_name" bash -lc './scripts/ensure_build_deps.sh'

status "Running in ${container_name}: $*"
runtime exec -w "$workspace_path" "$container_name" "$@"
