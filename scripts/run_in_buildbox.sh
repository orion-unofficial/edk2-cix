#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"
workspace_parent="/workspaces"
workspace_path="${workspace_parent}/$(basename -- "$repo_root")"
host_workspace_parent="$(dirname -- "$repo_root")"

container_name="${EDK2_CIX_BUILDBOX_NAME:-edk2-cix-buildbox}"
container_image="${EDK2_CIX_BUILDBOX_IMAGE:-mcr.microsoft.com/devcontainers/base:bookworm}"

status() {
    printf '[buildbox] %s\n' "$*"
}

ensure_container() {
    if docker container inspect "$container_name" >/dev/null 2>&1; then
        local mounts
        mounts="$(docker inspect -f '{{range .Mounts}}{{println .Destination}}{{end}}' "$container_name")"
        if ! grep -qx '/workspaces' <<<"$mounts" || ! grep -qx '/hosttmp' <<<"$mounts"; then
            status "Recreating ${container_name} with the expected workspace mounts"
            docker rm -f "$container_name" >/dev/null
        elif [[ "$(docker inspect -f '{{.State.Running}}' "$container_name")" != "true" ]]; then
            status "Starting existing container ${container_name}"
            docker start "$container_name" >/dev/null
            return 0
        else
            return 0
        fi
    fi

    status "Creating container ${container_name}"
    docker run -d \
        --name "$container_name" \
        --platform linux/amd64 \
        --ulimit nofile=1024:524288 \
        -v "${host_workspace_parent}:${workspace_parent}" \
        -v /private/tmp:/hosttmp \
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

ensure_container

status "Ensuring build dependencies are present"
docker exec -w "$workspace_path" "$container_name" bash -lc './scripts/ensure_build_deps.sh'

status "Running in ${container_name}: $*"
docker exec -w "$workspace_path" "$container_name" "$@"